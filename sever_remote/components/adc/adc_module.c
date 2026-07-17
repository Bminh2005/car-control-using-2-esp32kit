#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "adc_module.h" // Include file header của chính nó
static const char *TAG = "ADC_MODULE";
static QueueHandle_t app_adc_queue = NULL;
// 1. Task đọc ADC (Từ khóa 'static' giúp hàm này hoàn toàn tàng hình với các file khác)

static void adc_read_task(void *pvParameters)
{
    adc_oneshot_unit_handle_t adc;

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    adc_oneshot_config_channel(adc, ADC_CHANNEL_0, &chan_cfg); // VP PINOUT
    // adc_oneshot_config_channel(adc, ADC_CHANNEL_3, &chan_cfg); // VN PINOUT
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &chan_cfg); // D34 PINOUT

    while (1)
    {
        int x, y;
        // adc_oneshot_read(adc, ADC_CHANNEL_0, &dummy);
        adc_oneshot_read(adc, ADC_CHANNEL_0, &x);
        // adc_oneshot_read(adc, ADC_CHANNEL_3, &dummy);
        // adc_oneshot_read(adc, ADC_CHANNEL_3, &y);
        // adc_oneshot_read(adc, ADC_CHANNEL_6, &dummy);
        adc_oneshot_read(adc, ADC_CHANNEL_6, &y);

        // x = (x >> 7);
        // y = (y >> 7);
        // z = (z >> 7);
        adc_data_t data_to_send;
        data_to_send.x_val = (uint8_t)((x * 100) / 4095); // Chuyển sang phần trăm
        data_to_send.y_val = (uint8_t)((y * 100) / 4095); // Chuyển sang phần trăm
        // data_to_send.z_val = (uint8_t)((z * 100) / 4095); // Chuyển sang phần trăm
        // Dùng ESP_LOGI thay cho printf để có màu sắc và kèm theo thời gian thực (timestamp)
        if (app_adc_queue != NULL)
        {
            if (xQueueSend(app_adc_queue, &data_to_send, pdMS_TO_TICKS(10)) != pdPASS)
            {
                ESP_LOGW(TAG, "Queue day! Chua kip xu ly du lieu ADC");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 2. Hàm khởi tạo (Hàm này public ra bên ngoài)
void adc_module_init(QueueHandle_t output_queue)
{
    ESP_LOGI(TAG, "Khởi tạo ADC Module...");
    app_adc_queue = output_queue; // Lưu Queue lại để Task dùng

    // Khóa Task này vào Core 1 để không làm phiền Core 0 (Bluetooth)
    xTaskCreatePinnedToCore(adc_read_task, "ADC_Task", 4096, NULL, 5, NULL, 1);
}