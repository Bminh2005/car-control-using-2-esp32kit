#ifndef L298N_DRIVER_H
#define L298N_DRIVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 2. Cập nhật hàm init: Nhận vào một QueueHandle_t
void l298n_init(QueueHandle_t output_queue);

#endif // ADC_MODULE_H