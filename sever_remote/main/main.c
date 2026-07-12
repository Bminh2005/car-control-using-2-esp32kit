#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "../components/adc/adc_module.h"
#include "../protocol/my_protocol.h"
#define SPP_TAG "BT_SERVER_V5"
#define SPP_SERVER_NAME "SPP_SERVER"
#define DEVICE_NAME "ESP32_Server_BT"

// Handle của kết nối SPP để gửi data đi
static uint32_t spp_handle = 0;
// Queue để giao tiếp giữa BT Callback và FreeRTOS Task
static QueueHandle_t spp_data_queue = NULL;

// Cấu trúc dữ liệu nhét vào Queue
typedef struct
{
    uint8_t data[128];
    uint16_t len;
} spp_data_t;

// --------------------------------------------------------
// 1. FREERTOS TASK: Xử lý và Phản hồi
// --------------------------------------------------------
void bt_processing_task(void *pvParameters)
{
    spp_data_t rx_data;
    ESP_LOGI(SPP_TAG, "Task xu ly cua Server da chay!");
    QueueHandle_t queue = (QueueHandle_t)pvParameters; // Nhận Queue từ tham số
    adc_data_t received_data;
    char *task_name = pcTaskGetName(NULL);
    if (xQueueReceive(spp_data_queue, &rx_data, portMAX_DELAY))
    {
        rx_data.data[rx_data.len] = '\0'; // Đảm bảo kết thúc chuỗi
        ESP_LOGI(SPP_TAG, "Nhan duoc tu Client: %s", rx_data.data);

        // Gửi phản hồi lại Client (nếu đang có kết nối)
        if (spp_handle != 0)
        {
            char reply[64] = "Server xac nhan da nhan data!\n";
            esp_spp_write(spp_handle, strlen(reply), (uint8_t *)reply);
        }
    }
    while (1)
    {

        // Chờ nhận dữ liệu từ Queue

        if (xQueueReceive(queue, &received_data, portMAX_DELAY) == pdPASS)
        {

            ESP_LOGI(task_name, "Nhan duoc tu ADC: X=%d, Y=%d, Z=%d",
                     received_data.x_val, received_data.y_val, received_data.z_val);

            // TẠI ĐÂY: Bạn gọi hàm esp_spp_write() để gửi dữ liệu qua Bluetooth
            // esp_spp_write(handle, len, data);
            packet_adc_t packet;
            packet.msg_id = MSG_ID_ADC_DATA;
            packet.data = received_data;
            esp_spp_write(spp_handle, sizeof(packet), (uint8_t *)&packet);
        }
    }
}

// --------------------------------------------------------
// 2. BLUETOOTH CALLBACK: Hứng sự kiện
// --------------------------------------------------------
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "SPP khoi tao thanh cong, dang bat Server...");
            esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
        }
        break;

    case ESP_SPP_START_EVT:
        // Khi Server đã start xong, ta mới đổi tên và cho phép thiết bị khác quét thấy
        if (param->start.status == ESP_SPP_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "Server da bat! Dang phat song Bluetooth...");
            esp_bt_gap_set_device_name(DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        // Sự kiện này xảy ra khi có một Client kết nối thành công tới Server
        ESP_LOGI(SPP_TAG, "Client da ket noi toi Server!");
        spp_handle = param->srv_open.handle; // Lưu handle để gửi data lại
        break;

    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "Client da ngat ket noi");
        spp_handle = 0;
        break;

    case ESP_SPP_DATA_IND_EVT:
        // SỰ KIỆN NHẬN DỮ LIỆU -> Bắn vào FreeRTOS Queue
        if (param->data_ind.len < 128)
        {
            spp_data_t tx_data;
            tx_data.len = param->data_ind.len;
            memcpy(tx_data.data, param->data_ind.data, tx_data.len);
            xQueueSend(spp_data_queue, &tx_data, pdMS_TO_TICKS(10));
        }
        break;

    default:
        break;
    }
}

// --------------------------------------------------------
// 3. HÀM MAIN: Khởi tạo toàn bộ (Chuẩn v5.5)
// --------------------------------------------------------
void app_main(void)
{
    // 1. Khởi tạo bộ nhớ NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Giải phóng RAM của BLE (Chỉ dùng Classic BT)
    // ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // 3. Tạo FreeRTOS Queue
    spp_data_queue = xQueueCreate(10, sizeof(spp_data_t));

    // 4. Khởi tạo Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM)); // Dùng Dual Mode để có thể dùng cả Classic BT và BLE

    // 5. Khởi tạo Bluedroid (Dùng struct cho v5.x)
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // 6. Khởi tạo SPP (Dùng struct cho v5.x)
    ESP_ERROR_CHECK(esp_spp_register_callback(esp_spp_cb));

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&bt_spp_cfg));
    QueueHandle_t main_adc_queue = xQueueCreate(10, sizeof(adc_data_t));
    // 7. Khởi tạo FreeRTOS Task
    // xTaskCreate(bt_processing_task, "bt_processing_task", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(bt_processing_task, "BT_Task", 4096, (void *)main_adc_queue, 5, NULL, 1);
    adc_module_init(main_adc_queue); // Khởi tạo ADC Module
}