#ifndef ULTRASONIC_TASK_H
#define ULTRASONIC_TASK_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// -----------------------------------------------------------------------------
// 1. Cấu trúc dữ liệu để gửi qua Queue
// -----------------------------------------------------------------------------

/**
 * @brief Định nghĩa gói dữ liệu siêu âm để gửi đi xử lý.
 * * Lưu ý: Việc đưa trigger_pin vào gói dữ liệu giống như việc "gắn biển số"
 * để bên nhận (Task Bluetooth) biết được khoảng cách này là của cảm biến nào
 * (ví dụ: chân 5 là đầu xe, chân 19 là đuôi xe).
 */
typedef struct
{
    uint8_t trigger_pin; // Dùng để xác định nguồn gửi (ID cảm biến)
    float distance_cm;   // Khoảng cách đo được (đơn vị: Centimet)
} ultrasonic_data_t;

// -----------------------------------------------------------------------------
// 2. Khai báo các hàm giao tiếp (API)
// -----------------------------------------------------------------------------

/**
 * @brief Khởi tạo một cảm biến siêu âm và chạy nó trên một Task ngầm.
 * * Hàm này có thể được gọi nhiều lần trong app_main() để khởi tạo
 * nhiều cảm biến siêu âm khác nhau, mỗi cảm biến sẽ tự động được
 * gán cho một FreeRTOS Task riêng biệt để đo đạc song song.
 * * @param gpio_trigger Chân GPIO nối với chân Trig của HC-SR04
 * @param gpio_echo    Chân GPIO nối với chân Echo của HC-SR04
 * @param output_queue (Tùy chọn) Queue để Task tự động nhét kết quả đo được vào.
 * Truyền NULL nếu bạn chỉ muốn in ra Serial Monitor để test.
 */
void ultrasonic_task_init(uint8_t gpio_trigger, uint8_t gpio_echo, QueueHandle_t output_queue);
void ultrasonic_test(void *pvParameters);
#endif // ULTRASONIC_MODULE_H