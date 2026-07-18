#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "../../protocol/my_protocol.h"
static const char *TAG = "ROBOT_ALL_COMMANDS";

// Cấu hình UART giao tiếp PC
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE 1024

// Cấu hình chân GPIO nối Driver MX1508
#define MOTOR_IN1_PIN 25 // PWM Bánh Trái
#define MOTOR_IN2_PIN 26 // Hướng Bánh Trái
#define MOTOR_IN3_PIN 27 // PWM Bánh Phải
#define MOTOR_IN4_PIN 14 // Hướng Bánh Phải

// Cấu hình LEDC PWM
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT // Độ phân giải 8-bit (0-255)
#define LEDC_FREQ 5000                 // Tần số 5kHz giúp motor chạy êm

// Thời gian thực nghiệm để xe xoay được góc 90 độ (đơn vị: mili-giây)
// Bạn có thể tinh chỉnh số này tùy thuộc vào độ ma sát bề mặt sàn
#define TIME_TURN_90_MS 550

// QueueHandle_t command_queue = NULL;
// static QueueHandle_t stop_now_queue = NULL; // Queue dùng chung giữa BT và Driver
extern uint8_t stop_now; // Biến toàn cục dùng chung giữa các file để kiểm tra trạng thái dừng khẩn cấp
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
// void motor_control_task(void *pvParameters)
// {
//     adc_data_t rx_cmd;
//     uint32_t toc_do_chay = 180; // Tốc độ di chuyển tiến/lùi (0-255)
//     uint32_t toc_do_xoay = 255; // Tốc độ xoay tại chỗ (nên để cao để thắng ma sát sàn)
//     uint8_t stop_now = 0;
//     while (1)
//     {
//         // Đợi lệnh từ Queue vô hạn không tốn tài nguyên CPU ngầm
//         if (xQueueReceive(command_queue, &rx_cmd, portMAX_DELAY) == pdPASS)
//         {
//             uint8_t x, y, z;
//             x = rx_cmd.x_val;
//             y = rx_cmd.y_val;
//             z = rx_cmd.z_val;
//             printf("MOTOR: Nhận lệnh từ Queue: X=%d, Y=%d, Z=%d\n", x, y, z);

//             // case 'S': // XE DỪNG
//             //     ESP_LOGE(TAG, "MOTOR: Lệnh DỪNG");
//             //     brake_robot();
//             //     break;

//             if (z < 44)
//             { // XOAY TRÁI 90 ĐỘ TẠI CHỖ
//                 ESP_LOGW(TAG, "MOTOR: Đang xoay Trái 90 độ...");
//                 // Bánh trái LÙI, Bánh phải TIẾN
//                 gpio_set_level(MOTOR_IN2_PIN, 1);
//                 gpio_set_level(MOTOR_IN4_PIN, 0);
//                 ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 255 - toc_do_xoay * 0);
//                 ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, toc_do_xoay);
//                 ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
//                 ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);

//                 // Chờ chạy hết thời gian xoay góc rồi tự phanh
//                 // vTaskDelay(pdMS_TO_TICKS(TIME_TURN_90_MS));
//                 // brake_robot();
//                 ESP_LOGI(TAG, "MOTOR: Đã xoay xong Trái.");
//             }

//             else if (z > 47)
//             { // XOAY PHẢI 90 ĐỘ TẠI CHỖ
//                 ESP_LOGW(TAG, "MOTOR: Đang xoay Phải 90 độ...");
//                 // Bánh trái TIẾN, Bánh phải LÙI
//                 gpio_set_level(MOTOR_IN2_PIN, 0);
//                 gpio_set_level(MOTOR_IN4_PIN, 1);
//                 ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, toc_do_xoay);
//                 ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 255 - toc_do_xoay * 0);
//                 ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
//                 ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
//                 // brake_robot();
//                 ESP_LOGI(TAG, "MOTOR: Đã xoay xong Phải.");
//             }
//             else
//             {
//                 if (x > 46) // XE ĐI TIẾN
//                 {
//                     toc_do_chay = (x - 47) * 255 / 53; // Tốc độ tiến dựa trên chênh lệch x và z
//                     ESP_LOGW(TAG, "MOTOR: Lệnh TIẾN");
//                     gpio_set_level(MOTOR_IN2_PIN, 0);
//                     gpio_set_level(MOTOR_IN4_PIN, 0);
//                     ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, toc_do_chay);
//                     ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, toc_do_chay);
//                     ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
//                     ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
//                 }

//                 else if (x < 44) // XE ĐI LÙI
//                 {
//                     toc_do_chay = (44 - x) * 255 / 44;
//                     ESP_LOGW(TAG, "MOTOR: Lệnh LÙI");
//                     gpio_set_level(MOTOR_IN2_PIN, 1);
//                     gpio_set_level(MOTOR_IN4_PIN, 1);
//                     ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 255 - toc_do_chay);
//                     ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 255 - toc_do_chay);
//                     ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
//                     ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
//                 }
//                 else
//                 {
//                     brake_robot(); // Nếu x nằm trong khoảng 44-46 thì dừng xe
//                 }
//                 // brake_robot(); // Nếu z nằm trong khoảng 44-46 thì dừng xe
//             }
//         }
//     }
// }

void motor_control_task(void *pvParameters)
{
    // Nhận Queue truyền vào (chứa các packet nhận từ Bluetooth)
    QueueHandle_t command_queue = (QueueHandle_t)pvParameters;

    packet_control_data_t rx_packet;
    uint32_t toc_do_xoay = 200; // Tốc độ xoay tại chỗ (nên để cao để thắng ma sát sàn)

    ESP_LOGI(TAG, "MOTOR: Task dieu khien dong co nhan packet tu Bluetooth da chay!");

    while (1)
    {
        // Đợi lệnh từ Queue vô hạn không tốn tài nguyên CPU ngầm (portMAX_DELAY)
        if (xQueueReceive(command_queue, &rx_packet, portMAX_DELAY) == pdPASS)
        {
            // Kiểm tra xem có đúng ID gói tin điều khiển hay không
            if (rx_packet.msg_id == MSG_ID_CONTROL_DATA)
            {
                uint8_t huong_di = rx_packet.direction;
                uint8_t toc_do = rx_packet.speed;
                if (stop_now && huong_di == 1)
                {
                    ESP_LOGW(TAG, "MOTOR: Dừng khẩn cấp do cảm biến siêu âm!");
                    brake_robot();
                    continue; // Bỏ qua lệnh hiện tại và tiếp tục vòng lặp
                }
                printf("MOTOR: Nhận lệnh từ BT: Hướng=%d, Tốc độ=%d\n", huong_di, toc_do);

                switch (huong_di)
                {
                case 0: // XE DỪNG
                    ESP_LOGE(TAG, "MOTOR: Lệnh DỪNG");
                    brake_robot();
                    break;

                case 1: // XE ĐI TIẾN
                    ESP_LOGW(TAG, "MOTOR: Lệnh TIẾN - Tốc độ: %d", toc_do);
                    gpio_set_level(MOTOR_IN2_PIN, 0);
                    gpio_set_level(MOTOR_IN4_PIN, 0);
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, toc_do);
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, toc_do);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
                    break;

                case 2: // XE ĐI LÙI
                    ESP_LOGW(TAG, "MOTOR: Lệnh LÙI - Tốc độ: %d", toc_do);
                    gpio_set_level(MOTOR_IN2_PIN, 1);
                    gpio_set_level(MOTOR_IN4_PIN, 1);
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 255 - toc_do);
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 255 - toc_do);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
                    break;

                case 3: // XOAY TRÁI TẠI CHỖ
                    ESP_LOGW(TAG, "MOTOR: Đang xoay R 90 độ...");
                    // Bánh trái LÙI, Bánh phải TIẾN
                    gpio_set_level(MOTOR_IN2_PIN, 0);
                    gpio_set_level(MOTOR_IN4_PIN, 0);
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 100); // R
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 255); // L manh
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
                    ESP_LOGI(TAG, "MOTOR: Đã thực hiện lệnh xoay R");
                    break;

                case 4: // XOAY PHẢI TẠI CHỖ
                    ESP_LOGW(TAG, "MOTOR: Đang xoay L 90 độ...");
                    // Bánh trái TIẾN, Bánh phải LÙI
                    gpio_set_level(MOTOR_IN2_PIN, 0);
                    gpio_set_level(MOTOR_IN4_PIN, 0);
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 255); // R
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 80);  // L manh
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
                    ESP_LOGI(TAG, "MOTOR: Đã thực hiện lệnh xoay L.");
                    break;

                default:
                    brake_robot();
                    break;
                }
            }
        }
    }
}

void l298n_init(QueueHandle_t command_queue_in)
{
    init_uart();
    init_motors();
    // stop_now_queue = stop_queue; // Gán Queue nhận lệnh dừng khẩn cấp từ main.c
    // Tạo Queue chứa dữ liệu điều khiển
    // command_queue = xQueueCreate(10, sizeof(char));

    // Ghim cả 2 tác vụ hoạt động độc lập sang Core 1
    // xTaskCreatePinnedToCore(uart_rx_task, "uart_rx_task", 3072, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(motor_control_task, "motor_control_task", 4096, (void *)command_queue_in, 5, NULL, 1);
    // ESP_LOGI(TAG, "Hệ thống FreeRTOS tích hợp rẽ hướng đã khởi động xong!");
}