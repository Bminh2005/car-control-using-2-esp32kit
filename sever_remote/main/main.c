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
#include "freertos/timers.h" // Thêm thư viện quản lý Timer của FreeRTOS
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "../components/adc/adc_module.h"
#include "../protocol/my_protocol.h"
#include "driver/gpio.h"

#define SPP_TAG "BT_SERVER_V5"
#define SPP_SERVER_NAME "SPP_SERVER"
#define DEVICE_NAME "ESP32_Server_BT"
#define ESP_INTR_FLAG_DEFAULT 0

// Tần suất gửi gói tin khi nhấn giữ nút (ví dụ: 50ms gửi một lần)
#define BUTTON_SEND_PERIOD_MS 10
static TaskHandle_t shock_task_handle = NULL;

static uint32_t spp_handle = 0;
static QueueHandle_t spp_data_queue = NULL;
static QueueHandle_t gpio_evt_queue = NULL;

// Định nghĩa Timer điều khiển gửi nút bấm liên tục
static TimerHandle_t btn_32_timer = NULL;
static TimerHandle_t btn_33_timer = NULL;
static uint8_t is_shocking = 0; // Biến cờ để kiểm tra trạng thái shock
typedef struct
{
    uint8_t data[128];
    uint16_t len;
} spp_data_t;

// --------------------------------------------------------
// 1. TIMER CALLBACKS: Tự động bắn dữ liệu định kỳ khi giữ nút
// --------------------------------------------------------
static void btn_32_timer_callback(TimerHandle_t xTimer)
{
    if (spp_handle != 0)
    {
        packet_control_data_t packet;
        packet.msg_id = MSG_ID_CONTROL_DATA;
        packet.direction = 4; // Xoay Phai
        packet.speed = 0;
        esp_spp_write(spp_handle, sizeof(packet), (uint8_t *)&packet);
        ESP_LOGD(SPP_TAG, "Timer 32: Dang gui lenh HUONG 3...");
    }
}

static void btn_33_timer_callback(TimerHandle_t xTimer)
{
    if (spp_handle != 0)
    {
        packet_control_data_t packet;
        packet.msg_id = MSG_ID_CONTROL_DATA;
        packet.direction = 3; // Xoay trai
        packet.speed = 0;
        esp_spp_write(spp_handle, sizeof(packet), (uint8_t *)&packet);
        ESP_LOGD(SPP_TAG, "Timer 33: Dang gui lenh HUONG 4...");
    }
}

// --------------------------------------------------------
// 2. INTERRUPT HANDLER: Bắt sự kiện Nhấn xuống / Thả ra
// --------------------------------------------------------
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Đẩy trạng thái thay đổi chân vào Queue để Task xử lý (tránh xử lý logic trong ISR)
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void process_received_data(uint8_t *data_in, uint16_t len)
{
    if (len == 0)
        return;

    // Đọc byte đầu tiên (msg_id)
    uint8_t id = data_in[0];

    switch (id)
    {
    case MSG_ID_SHOCK_EVENT:
    {
        packet_shock_t *shock_packet = (packet_shock_t *)data_in;
        ESP_LOGI("RX", "Nhan su kien shock: %s", shock_packet->is_shock == 1 ? "CO" : "KHONG");

        // Nếu có sự kiện shock và shock_task đã được tạo thành công
        if (shock_packet->is_shock == 1 && shock_task_handle != NULL)
        {
            xTaskNotifyGive(shock_task_handle); // Đánh thức shock_task ngay lập tức
        }
        break;
    }
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

void shock_task(void *pvParameters)
{
    // Đảm bảo chân GPIO2 được khởi tạo ở mức thấp ban đầu
    gpio_set_level(GPIO_NUM_5, 0);

    while (1)
    {
        // Chờ thông báo từ task nhận dữ liệu (block vô hạn, không tốn CPU)
        // pdTRUE: Reset giá trị notification về 0 sau khi nhận được
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGI("SHOCK", "Kích hoạt SHOCK trong 1000ms...");
        gpio_set_level(GPIO_NUM_5, 1);   // Bật chân GPIO2
        vTaskDelay(pdMS_TO_TICKS(1000)); // Giữ trong 1s
        gpio_set_level(GPIO_NUM_5, 0);   // Tắt chân GPIO2
    }
}
// --------------------------------------------------------
// 3. FREERTOS TASK: Xử lý sự kiện nút bấm & Đọc ADC
// --------------------------------------------------------
void bt_processing_task(void *pvParameters)
{
    spp_data_t rx_data;
    ESP_LOGI(SPP_TAG, "Task xu ly cua Server da chay!");
    QueueHandle_t queue = (QueueHandle_t)pvParameters; // Nhận Queue ADC từ tham số
    adc_data_t received_data;
    uint32_t btn_pin;
    char *task_name = pcTaskGetName(NULL);

    // Xử lý gói tin nhận được từ Client ban đầu (nếu có)
    if (xQueueReceive(spp_data_queue, &rx_data, 0))
    {
        rx_data.data[rx_data.len] = '\0';
        ESP_LOGI(SPP_TAG, "Nhan duoc tu Client: %s", rx_data.data);
        if (spp_handle != 0)
        {
            char reply[64] = "Server xac nhan da nhan data!\n";
            esp_spp_write(spp_handle, strlen(reply), (uint8_t *)reply);
        }
    }

    while (1)
    {
        if (xQueueReceive(spp_data_queue, &rx_data, 0) == pdPASS)
        {
            // Gọi hàm xử lý gói tin và kích hoạt shock nếu cần
            process_received_data(rx_data.data, rx_data.len);
        }
        // ==========================================
        // QUÉT NGẮT NÚT BẤM (CẢ NHẤN VÀ NHẢ)
        // ==========================================
        while (xQueueReceive(gpio_evt_queue, &btn_pin, 0) == pdPASS)
        {
            // Đọc trạng thái vật lý tức thời của chân GPIO ngay khi xảy ra ngắt
            int pin_state = gpio_get_level(btn_pin);

            if (btn_pin == GPIO_NUM_32)
            {
                if (pin_state == 1) // Nút 32 ĐƯỢC NHẤN XUỐNG (Pull-down vọt lên 1)
                {
                    ESP_LOGI(task_name, "[NUT 32] -> NHẤN GIỮ");
                    // Kích hoạt Timer gửi liên tục của chân 32
                    xTimerStart(btn_32_timer, 0);
                }
                else // Nút 32 ĐƯỢC NHẢ RA (Quay về 0)
                {
                    ESP_LOGI(task_name, "[NUT 32] -> NHẢ");
                    // Dừng ngay lập tức Timer của chân 32
                    xTimerStop(btn_32_timer, 0);
                }
            }
            else if (btn_pin == GPIO_NUM_33)
            {
                if (pin_state == 1) // Nút 33 ĐƯỢC NHẤN XUỐNG
                {
                    ESP_LOGI(task_name, "[NUT 33] -> NHẤN GIỮ");
                    xTimerStart(btn_33_timer, 0);
                }
                else // Nút 33 ĐƯỢC NHẢ RA
                {
                    ESP_LOGI(task_name, "[NUT 33] -> NHẢ");
                    xTimerStop(btn_33_timer, 0);
                }
            }
        }

        // ==========================================
        // ĐỌC DỮ LIỆU TỪ ADC (CHỈ ĐỌC KHI KHÔNG GIỮ NÚT)
        // ==========================================
        if (xQueueReceive(queue, &received_data, pdMS_TO_TICKS(10)) == pdPASS)
        {
            // Chỉ gửi dữ liệu ADC nếu cả hai Timer nút bấm đều đang DỪNG (không bị giữ nút bấm đè lên)
            if (xTimerIsTimerActive(btn_32_timer) == pdFALSE && xTimerIsTimerActive(btn_33_timer) == pdFALSE)
            {
                uint8_t x = received_data.x_val;
                uint8_t y = received_data.y_val;
                // printf("X: %d, Y: %d\n", x, y);
                packet_control_data_t packet;
                packet.msg_id = MSG_ID_CONTROL_DATA;

                if (x > y + 2)
                {
                    packet.direction = 1; // Tiến
                    packet.speed = 255 * (x - y) / (100 - y);
                }
                else if (x < y - 2)
                {
                    packet.direction = 2; // Lùi
                    packet.speed = 255 * (y - x) / (100 - x);
                }
                else
                {
                    packet.direction = 0; // Dừng xe
                    packet.speed = 0;
                }

                if (spp_handle != 0)
                {
                    esp_spp_write(spp_handle, sizeof(packet), (uint8_t *)&packet);
                }
            }
        }
    }
}

// --------------------------------------------------------
// 4. BLUETOOTH CALLBACK (Giữ nguyên)
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
        if (param->start.status == ESP_SPP_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "Server da bat! Dang phat song Bluetooth...");
            esp_bt_gap_set_device_name(DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "Client da ket noi toi Server!");
        spp_handle = param->srv_open.handle;
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "Client da ngat ket noi");
        spp_handle = 0;
        // Dừng khẩn cấp các timer nếu client mất kết nối khi đang giữ nút
        xTimerStop(btn_32_timer, 0);
        xTimerStop(btn_33_timer, 0);
        break;
    case ESP_SPP_DATA_IND_EVT:
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

void init_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_32) | (1ULL << GPIO_NUM_33),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Bật kéo xuống mặc định mức 0
        .intr_type = GPIO_INTR_ANYEDGE        // QUAN TRỌNG: Ngắt ở CẢ 2 CẠNH (Nhấn lên 1 ngắt, Nhả xuống 0 ngắt)
    };
    gpio_config(&io_conf);

    gpio_config_t io_conf_output = {
        .pin_bit_mask = (1ULL << GPIO_NUM_5),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_output);

    // Đặt trạng thái ban đầu của GPIO2 về 0 (Tắt bộ rung/LED)
    gpio_set_level(GPIO_NUM_5, 0);
}

// --------------------------------------------------------
// 5. HÀM MAIN: Khởi tạo toàn bộ
// --------------------------------------------------------
void app_main(void)
{
    // Tạo Queue nhận ngắt nút nhấn
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (gpio_evt_queue == NULL)
    {
        ESP_LOGE(SPP_TAG, "Khong the khoi tao gpio_evt_queue!");
        return;
    }

    // Khởi tạo các phần mềm Software Timers cho 2 nút bấm
    btn_32_timer = xTimerCreate("Btn_32_Timer", pdMS_TO_TICKS(BUTTON_SEND_PERIOD_MS), pdTRUE, (void *)0, btn_32_timer_callback);
    btn_33_timer = xTimerCreate("Btn_33_Timer", pdMS_TO_TICKS(BUTTON_SEND_PERIOD_MS), pdTRUE, (void *)1, btn_33_timer_callback);

    if (btn_32_timer == NULL || btn_33_timer == NULL)
    {
        ESP_LOGE(SPP_TAG, "Khong the tao FreeRTOS Timers!");
        return;
    }

    // Cài đặt dịch vụ ngắt GPIO
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // Cấu hình GPIO và đăng ký ISR cho từng chân
    init_gpio();
    gpio_isr_handler_add(GPIO_NUM_32, gpio_isr_handler, (void *)GPIO_NUM_32);
    gpio_isr_handler_add(GPIO_NUM_33, gpio_isr_handler, (void *)GPIO_NUM_33);

    // Khởi tạo NVS và Bluetooth (Giữ nguyên cấu trúc v5.x ổn định của bạn)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    spp_data_queue = xQueueCreate(10, sizeof(spp_data_t));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_spp_register_callback(esp_spp_cb));

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&bt_spp_cfg));

    QueueHandle_t main_adc_queue = xQueueCreate(10, sizeof(adc_data_t));
    xTaskCreatePinnedToCore(bt_processing_task, "BT_Task", 4096, (void *)main_adc_queue, 5, NULL, 1);
    // Thay đổi dòng tạo task shock trong app_main thành:
    xTaskCreatePinnedToCore(shock_task, "shock_Task", 2048, NULL, 5, &shock_task_handle, 1);
    adc_module_init(main_adc_queue);
}