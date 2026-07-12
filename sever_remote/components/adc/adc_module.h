#ifndef ADC_MODULE_H
#define ADC_MODULE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 1. Định nghĩa gói dữ liệu (Ví dụ dùng 8-bit như đã bàn ở trên)
typedef struct
{
    uint8_t x_val;
    uint8_t y_val;
    uint8_t z_val;
} adc_data_t;

// 2. Cập nhật hàm init: Nhận vào một QueueHandle_t
void adc_module_init(QueueHandle_t output_queue);

#endif // ADC_MODULE_H