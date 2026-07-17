#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mpu6050.h"

// Cấu hình chân
#define SDA_PIN 21
#define SCL_PIN 22
#define ADDR MPU6050_I2C_ADDRESS_LOW // 0x68

// Ngưỡng phát hiện rung xóc (0.5g là độ nhạy khá tốt cho robot)
#define SHOCK_THRESHOLD 0.5f

static const char *TAG = "SHOCK_MODULE";

void mpu_shock_task(void *pvParameters)
{
    mpu6050_dev_t dev = {0};
    QueueHandle_t xShockQueue = (QueueHandle_t)pvParameters;
    // Khởi tạo thiết bị
    ESP_ERROR_CHECK(mpu6050_init_desc(&dev, ADDR, 0, SDA_PIN, SCL_PIN));
    ESP_ERROR_CHECK(mpu6050_init(&dev));

    ESP_LOGI(TAG, "Module shock da san sang!");

    while (1)
    {
        mpu6050_acceleration_t accel = {0};
        mpu6050_rotation_t rotation = {0};

        // Đọc dữ liệu thô
        if (mpu6050_get_motion(&dev, &accel, &rotation) == ESP_OK)
        {
            // Trục Z mặc định là 1.0 (hoặc -1.0) do trọng lực Trái đất
            // Ta lấy giá trị tuyệt đối của nó trừ đi 1 để tìm gia tốc văng (Linear Accel)
            float linear_z = fabs(accel.z) - 1.0f;

            if (fabs(linear_z) > SHOCK_THRESHOLD)
            {
                ESP_LOGW(TAG, "--- PHAT HIEN RUNG XOC! Z: %.2f g ---", linear_z);

                uint8_t event = 111; // Giá trị bất kỳ để báo hiệu rung xóc

                // Gửi dữ liệu vào Queue (không đợi nếu Queue đầy - xTicksToWait = 0)
                if (xShockQueue != NULL)
                {
                    if (xQueueSend(xShockQueue, &event, 0) != pdPASS)
                    {
                        ESP_LOGE(TAG, "Queue day, mat du lieu rung xoc!");
                    }
                }
                // Tránh log liên tục trong 500ms
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // Chạy ở 50Hz
    }
}

void mpu_task_init(QueueHandle_t xShockQueue)
{
    ESP_ERROR_CHECK(i2cdev_init());
    // Đảm bảo i2cdev đã khởi tạo trong main.c
    xTaskCreate(mpu_shock_task, "mpu_shock_task", 4096, (void *)xShockQueue, 6, NULL);
}