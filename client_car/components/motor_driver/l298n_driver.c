#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "ROBOT_ALL_COMMANDS";

// Cấu hình UART giao tiếp PC
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE 1024

// Cấu hình chân GPIO nối Driver MX1508
#define MOTOR_IN1_PIN 34 // PWM Bánh Trái
#define MOTOR_IN2_PIN 35 // Hướng Bánh Trái
#define MOTOR_IN3_PIN 32 // PWM Bánh Phải
#define MOTOR_IN4_PIN 33 // Hướng Bánh Phải

// Cấu hình LEDC PWM
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT // Độ phân giải 8-bit (0-255)
#define LEDC_FREQ 5000                 // Tần số 5kHz giúp motor chạy êm

// Thời gian thực nghiệm để xe xoay được góc 90 độ (đơn vị: mili-giây)
// Bạn có thể tinh chỉnh số này tùy thuộc vào độ ma sát bề mặt sàn
#define TIME_TURN_90_MS 550

QueueHandle_t command_queue = NULL;

// Hàm khởi tạo cấu hình PWM và GPIO cho Motor
void init_motors()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQ,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    // Cấu hình kênh PWM cho IN1 (Bánh Trái)
    ledc_channel_config_t ledc_ch_left = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = MOTOR_IN1_PIN,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_ch_left);

    // Cấu hình kênh PWM cho IN3 (Bánh Phải)
    ledc_channel_config_t ledc_ch_right = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = MOTOR_IN3_PIN,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_ch_right);

    // Cấu hình các chân hướng IN2 và IN4 làm GPIO Output tiêu chuẩn
    gpio_reset_pin(MOTOR_IN2_PIN);
    gpio_set_direction(MOTOR_IN2_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(MOTOR_IN4_PIN);
    gpio_set_direction(MOTOR_IN4_PIN, GPIO_MODE_OUTPUT);
}

// Hàm khởi tạo bộ UART kết nối với máy tính PC
void init_uart()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(EX_UART_NUM, &uart_config);
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
}

// Hàm hỗ trợ phanh dừng xe khẩn cấp
void brake_robot()
{
    gpio_set_level(MOTOR_IN2_PIN, 0);
    gpio_set_level(MOTOR_IN4_PIN, 0);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 0);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
}

// TASK 1: Đọc lệnh từ cổng Serial PC và đẩy vào Queue
// void uart_rx_task(void *pvParameters)
// {
//     uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
//     while (1)
//     {
//         int len = uart_read_bytes(EX_UART_NUM, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
//         if (len > 0)
//         {
//             char cmd = data[0]; // Nhận ký tự đầu tiên
//             ESP_LOGI(TAG, "UART: Đã nhận lệnh '%c'", cmd);
//             xQueueSend(command_queue, &cmd, 0);
//         }
//         vTaskDelay(pdMS_TO_TICKS(50));
//     }
//     free(data);
// }

// TASK 2: Xử lý các trạng thái di chuyển dựa trên Queue nhận được
void motor_control_task(void *pvParameters)
{
    char rx_cmd;
    uint32_t toc_do_chay = 180; // Tốc độ di chuyển tiến/lùi (0-255)
    uint32_t toc_do_xoay = 210; // Tốc độ xoay tại chỗ (nên để cao để thắng ma sát sàn)

    while (1)
    {
        // Đợi lệnh từ Queue vô hạn không tốn tài nguyên CPU ngầm
        if (xQueueReceive(command_queue, &rx_cmd, portMAX_DELAY) == pdPASS)
        {
            switch (rx_cmd)
            {
            case 'F': // XE ĐI TIẾN
                ESP_LOGW(TAG, "MOTOR: Lệnh TIẾN");
                gpio_set_level(MOTOR_IN2_PIN, 0);
                gpio_set_level(MOTOR_IN4_PIN, 0);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, toc_do_chay);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, toc_do_chay);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
                break;

            case 'B': // XE ĐI LÙI
                ESP_LOGW(TAG, "MOTOR: Lệnh LÙI");
                gpio_set_level(MOTOR_IN2_PIN, 1);
                gpio_set_level(MOTOR_IN4_PIN, 1);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 255 - toc_do_chay);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 255 - toc_do_chay);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
                break;

            case 'S': // XE DỪNG
                ESP_LOGE(TAG, "MOTOR: Lệnh DỪNG");
                brake_robot();
                break;

            case 'L': // XOAY TRÁI 90 ĐỘ TẠI CHỖ
                ESP_LOGW(TAG, "MOTOR: Đang xoay Trái 90 độ...");
                // Bánh trái LÙI, Bánh phải TIẾN
                gpio_set_level(MOTOR_IN2_PIN, 1);
                gpio_set_level(MOTOR_IN4_PIN, 0);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 255 - toc_do_xoay);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, toc_do_xoay);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);

                // Chờ chạy hết thời gian xoay góc rồi tự phanh
                vTaskDelay(pdMS_TO_TICKS(TIME_TURN_90_MS));
                brake_robot();
                ESP_LOGI(TAG, "MOTOR: Đã xoay xong Trái.");
                break;

            case 'R': // XOAY PHẢI 90 ĐỘ TẠI CHỖ
                ESP_LOGW(TAG, "MOTOR: Đang xoay Phải 90 độ...");
                // Bánh trái TIẾN, Bánh phải LÙI
                gpio_set_level(MOTOR_IN2_PIN, 0);
                gpio_set_level(MOTOR_IN4_PIN, 1);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, toc_do_xoay);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 255 - toc_do_xoay);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);

                // Chờ chạy hết thời gian xoay góc rồi tự phanh
                vTaskDelay(pdMS_TO_TICKS(TIME_TURN_90_MS));
                brake_robot();
                ESP_LOGI(TAG, "MOTOR: Đã xoay xong Phải.");
                break;

            default:
                ESP_LOGI(TAG, "MOTOR: Ký tự điều khiển không hợp lệ.");
                break;
            }
        }
    }
}

void l298n_init(QueueHandle_t command_queue_in)
{
    init_uart();
    init_motors();
    command_queue = command_queue_in;
    // Tạo Queue chứa dữ liệu điều khiển
    // command_queue = xQueueCreate(10, sizeof(char));

    if (command_queue != NULL)
    {
        // Ghim cả 2 tác vụ hoạt động độc lập sang Core 1
        // xTaskCreatePinnedToCore(uart_rx_task, "uart_rx_task", 3072, NULL, 3, NULL, 1);
        xTaskCreatePinnedToCore(motor_control_task, "motor_control_task", 3072, NULL, 2, NULL, 1);
        // ESP_LOGI(TAG, "Hệ thống FreeRTOS tích hợp rẽ hướng đã khởi động xong!");
    }
}