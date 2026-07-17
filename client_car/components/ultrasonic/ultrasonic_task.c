#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h> // Thêm thư viện này để dùng malloc
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ultrasonic.h>
#include <esp_err.h>
#include <ultrasonic_task.h>
#define MAX_DISTANCE_CM 500 // 5m max
// QueueHandle_t stop_now_queue = NULL;
extern uint8_t stop_now;
// 1. TASK ĐỌC CẢM BIẾN
void ultrasonic_test(void *pvParameters)
{
    // Ép kiểu tham số truyền vào thành con trỏ struct cấu hình
    ultrasonic_sensor_t *sensor = (ultrasonic_sensor_t *)pvParameters;

    // Khởi tạo chân GPIO cho cảm biến này
    ultrasonic_init(sensor);

    // Lấy thông tin chân để in log cho dễ phân biệt
    uint8_t t_pin = sensor->trigger_pin;
    uint8_t e_pin = sensor->echo_pin;

    while (true)
    {
        float distance;
        esp_err_t res = ultrasonic_measure(sensor, MAX_DISTANCE_CM, &distance);

        if (res != ESP_OK)
        {
            printf("[HC-SR04 | T:%d E:%d] Error %d: ", t_pin, e_pin, res);
            switch (res)
            {
            case ESP_ERR_ULTRASONIC_PING:
                printf("Cannot ping (invalid state)\n");
                break;
            case ESP_ERR_ULTRASONIC_PING_TIMEOUT:
                printf("Ping timeout (no device)\n");
                break;
            case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT:
                printf("Echo timeout (distance too big)\n");
                break;
            default:
                printf("%s\n", esp_err_to_name(res));
            }
        }
        else
        {
            // In ra khoảng cách kèm theo tên chân cắm để biết cảm biến nào đang đọc
            printf("[HC-SR04 | T:%d E:%d] Distance: %0.2f cm\n", t_pin, e_pin, distance * 100);
            if (t_pin == 5)
            {
                if (distance < 0.15)
                {
                    stop_now = 1;
                }
                else
                {
                    stop_now = 0;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// 2. HÀM KHỞI TẠO TỪNG CẢM BIẾN
void ultrasonic_task_init(uint8_t gpio_trigger, uint8_t gpio_echo)
{
    // Cấp phát một vùng nhớ độc lập cho cảm biến này
    ultrasonic_sensor_t *new_sensor = malloc(sizeof(ultrasonic_sensor_t));
    if (new_sensor == NULL)
    {
        printf("Loi: Khong du RAM de khoi tao cam bien!\n");
        return;
    } // Gán Queue nhận lệnh dừng khẩn cấp từ main.c
    // Gán thông số chân cắm vào struct
    new_sensor->trigger_pin = gpio_trigger;
    new_sensor->echo_pin = gpio_echo;

    // Tạo tên Task động cho dễ debug (vd: "usonic_5_18")
    char task_name[32];
    snprintf(task_name, sizeof(task_name), "usonic_%d_%d", gpio_trigger, gpio_echo);

    // Tạo Task và truyền con trỏ cấu hình vào pvParameters
    // Nhớ trói vào Core 1 (APP_CPU) để không cản trở Bluetooth
    xTaskCreatePinnedToCore(
        ultrasonic_test,              // Hàm chạy
        task_name,                    // Tên Task
        configMINIMAL_STACK_SIZE * 3, // Stack
        (void *)new_sensor,           // <-- TRUYỀN CẤU HÌNH VÀO TASK Ở ĐÂY
        5,                            // Mức ưu tiên
        NULL,                         // Task handle
        1                             // Chạy trên Core 1
    );
}