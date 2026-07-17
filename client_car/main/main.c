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
#include "../protocol/my_protocol.h"
#include "driver/gpio.h"
#include "../components/motor_driver/l298n_driver.h"
#include "../components/ultrasonic/ultrasonic_task.h"
#include "../components/mpu_shock_module/mpu_shock_module.h"
#define SPP_TAG "BT_CLIENT_V5"
// MAC Address của Server
uint8_t server_mac[6] = {0x70, 0x4B, 0xCA, 0x5D, 0xE0, 0xA6};
uint8_t stop_now = 0;
static uint32_t spp_handle = 0;
static QueueHandle_t spp_data_queue = NULL;
static QueueHandle_t main_control_queue = NULL;
static QueueHandle_t xShockQueue = NULL; // Queue dùng chung giữa MPU và Task chính
// static QueueHandle_t stop_now_queue = NULL;
typedef struct
{
    uint8_t data[128];
    uint16_t len;
} spp_data_t;

void init_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_2) | (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_5), // Chọn chân GPIO_NUM_2 (sử dụng bitmask)
        .mode = GPIO_MODE_OUTPUT,                                                           // Cấu hình làm ngõ ra
        .pull_up_en = GPIO_PULLUP_DISABLE,                                                  // Không bật điện trở kéo lên
        .pull_down_en = GPIO_PULLDOWN_DISABLE,                                              // Không bật điện trở kéo xuống
        .intr_type = GPIO_INTR_DISABLE                                                      // Tắt ngắt (interrupt) cho chân này
    };
    // Áp dụng cấu hình cấu trúc trên vào hệ thống
    gpio_config(&io_conf);
}
void process_received_data(uint8_t *data_in, uint16_t len)
{
    if (len == 0)
        return;

    // Đọc byte đầu tiên (msg_id)
    uint8_t id = data_in[0];

    switch (id)
    {
    case MSG_ID_CONTROL_DATA:
        // Nếu đúng kích thước
        if (len == sizeof(packet_control_data_t))
        {
            // Ép mảng byte thành struct ADC
            packet_control_data_t *control_packet = (packet_control_data_t *)data_in;
            // ESP_LOGI("RX", "Nhan ADC: X=%d, Y=%d, Z =%d", adc_packet->data.x_val, adc_packet->data.y_val, adc_packet->data.z_val);
            xQueueSend(main_control_queue, control_packet, pdMS_TO_TICKS(10)); // Gửi dữ liệu vào Queue để xử lý trong task điều khiển động cơ
        }
        break;

    case MSG_ID_TEXT_ACK:
        // Ép mảng byte thành struct Text
        packet_text_t *text_packet = (packet_text_t *)data_in;
        // Đảm bảo kết thúc chuỗi an toàn
        text_packet->text[sizeof(text_packet->text) - 1] = '\0';
        ESP_LOGI("RX", "Nhan phan hoi: %s", text_packet->text);
        break;

    default:
        ESP_LOGW("RX", "Goi tin khong xac dinh ID: 0x%02X", id);
        break;
    }
}
// --- TASK XỬ LÝ FREERTOS ---
void bt_processing_task(void *pvParameters)
{
    spp_data_t rx_data;
    ESP_LOGI(SPP_TAG, "Task xu ly cua Client da chay!");

    while (1)
    {
        // if (xQueueReceive(spp_data_queue, &rx_data, portMAX_DELAY))
        // {
        //     rx_data.data[rx_data.len] = '\0';
        //     ESP_LOGI(SPP_TAG, "Nhan duoc tu Server: %s", rx_data.data);
        // }
        // Chờ vô thời hạn cho đến khi Callback đẩy dữ liệu vào Queue
        if (xQueueReceive(spp_data_queue, &rx_data, portMAX_DELAY) == pdPASS)
        {
            // TUYỆT ĐỐI KHÔNG DÙNG: rx_data.data[rx_data.len] = '\0'; ở đây nữa
            // Vì đây là dữ liệu nhị phân, việc thêm '\0' có thể làm hỏng data.

            // Chuyền mảng byte thô và độ dài vào hàm giải mã
            process_received_data(rx_data.data, rx_data.len);
        }
        uint8_t shock_value;
        if (xQueueReceive(xShockQueue, &shock_value, 0) == pdPASS)
        {
            if (shock_value == 111)
            {
                packet_shock_t shock_packet;
                shock_packet.msg_id = MSG_ID_SHOCK_EVENT;
                shock_packet.is_shock = 1;
                // Gửi gói tin về Server ngay lập tức
                esp_err_t err = esp_spp_write(spp_handle, sizeof(shock_packet), (uint8_t *)&shock_packet);
                if (err == ESP_OK)
                {
                    ESP_LOGW(SPP_TAG, "--- DA GUI GOI TIN: SHOCK EVENT ve Server! ---");
                }
                else
                {
                    ESP_LOGE(SPP_TAG, "Gui goi tin SHOCK that bai! Error: %d", err);
                }
                ESP_LOGW(SPP_TAG, "Goi tin: SHOCK EVENT!");
            }
        }
    }
}

// --- CALLBACK ---
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(SPP_TAG, "SPP Client da khoi tao. Dang quet dich vu (Discovery) tren Server...");
        // BƯỚC 1: Gọi hàm dò tìm dịch vụ trước thay vì kết nối thẳng
        esp_spp_start_discovery(server_mac);
        break;

    case ESP_SPP_DISCOVERY_COMP_EVT:
        // BƯỚC 2: Sự kiện này nhảy vào khi quét xong
        if (param->disc_comp.status == ESP_SPP_SUCCESS)
        {
            // Lấy ra đúng số kênh (SCN) mà Server đang sử dụng
            uint8_t scn_channel = param->disc_comp.scn[0];
            ESP_LOGI(SPP_TAG, "Tim thay dich vu SPP tai kenh SCN: %d. Dang tien hanh ket noi...", scn_channel);

            // Thực hiện kết nối với đúng số kênh SCN vừa tìm được
            esp_spp_connect(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER, scn_channel, server_mac);
        }
        else
        {
            ESP_LOGE(SPP_TAG, "Quet that bai! Khong tim thay kenh SPP nao tren Server.");
        }
        break;

    case ESP_SPP_OPEN_EVT:
        // Sự kiện này nhảy vào khi KẾT NỐI THÀNH CÔNG
        ESP_LOGI(SPP_TAG, "Ket noi thanh cong toi Server!");
        spp_handle = param->open.handle;

        // Thử gửi một tin nhắn
        char *msg = "Xin chao tu Client v5!\n";
        esp_spp_write(spp_handle, strlen(msg), (uint8_t *)msg);
        break;

    case ESP_SPP_DATA_IND_EVT:
        // Nhận dữ liệu từ Server gửi về
        if (param->data_ind.len < 128)
        {
            spp_data_t tx_data;
            tx_data.len = param->data_ind.len;
            memcpy(tx_data.data, param->data_ind.data, tx_data.len);
            xQueueSend(spp_data_queue, &tx_data, pdMS_TO_TICKS(10));
        }
        break;

    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "Da ngat ket noi voi Server");
        spp_handle = 0;
        break;

    default:
        break;
    }
}

void app_main(void)
{
    init_gpio();
    xShockQueue = xQueueCreate(10, sizeof(uint8_t)); // Tạo Queue dùng chung giữa MPU và Task chính
    mpu_task_init(xShockQueue);                      // Khởi tạo Task đọc MPU6050
    // gpio_set_level(GPIO_NUM_2, 1); // Bật LED báo hiệu ESP32 đã khởi động xong
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Giải phóng RAM cho BLE vì chúng ta chỉ dùng Classic
    // ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    spp_data_queue = xQueueCreate(10, sizeof(spp_data_t));

    // 1. Controller Init (Chuẩn v5)
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));

    // 2. Bluedroid Init (Chuẩn v5 - Sửa lỗi sai trước đó)
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // 3. SPP Init (Chuẩn v5 - Sử dụng Struct)
    ESP_ERROR_CHECK(esp_spp_register_callback(esp_spp_cb));

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&bt_spp_cfg));
    main_control_queue = xQueueCreate(10, sizeof(packet_control_data_t));
    // stop_now_queue = xQueueCreate(10, sizeof(uint8_t)); // Queue dùng chung giữa BT và Driver
    l298n_init(main_control_queue); // Truyền Queue vào hàm init của driver
    ultrasonic_task_init(5, 18);    // Khởi tạo cảm biến siêu âm đầu xe (Trigger=5, Echo=18)
    // ultrasonic_task_init(19, 21, NULL);
    // 4. Chạy Task (Trói vào Core 1)
    xTaskCreatePinnedToCore(
        bt_processing_task,
        "BT_Process",
        4096,
        NULL,
        5,
        NULL,
        1 // <-- Chạy trên APP_CPU (Core 1)
    );
}