#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "adc_module.h" // Include file header của chính nó

static const char *TAG = "ADC_MODULE";

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
    adc_oneshot_config_channel(adc, ADC_CHANNEL_3, &chan_cfg); // VN PINOUT
    adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &chan_cfg); // D34 PINOUT

    while (1)
    {
        int x, y, z;
        int dummy;
        adc_oneshot_read(adc, ADC_CHANNEL_0, &dummy);
        adc_oneshot_read(adc, ADC_CHANNEL_0, &x);
        adc_oneshot_read(adc, ADC_CHANNEL_3, &dummy);
        adc_oneshot_read(adc, ADC_CHANNEL_3, &y);
        adc_oneshot_read(adc, ADC_CHANNEL_6, &dummy);
        adc_oneshot_read(adc, ADC_CHANNEL_6, &z);

        // x = (x >> 7);
        // y = (y >> 7);
        // z = (z >> 7);
        x = (x * 100) / 4095; // Chuyển sang phần trăm
        y = (y * 100) / 4095; // Chuyển sang phần trăm
        z = (z * 100) / 4095; // Chuyển sang phần trăm
        // Dùng ESP_LOGI thay cho printf để có màu sắc và kèm theo thời gian thực (timestamp)
        ESP_LOGI(TAG, "X=%4d  Y=%4d  Z=%4d", x, y, z);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 2. Hàm khởi tạo (Hàm này public ra bên ngoài)
void adc_module_init(void)
{
    ESP_LOGI(TAG, "Dang khoi tao ADC Module...");

    // Tự sinh ra Task
    // Tham số cuối cùng '1' nghĩa là bắt buộc Task này chỉ được phép chạy trên Core 1 (CPU1)
    xTaskCreatePinnedToCore(
        adc_read_task,   // Hàm chạy
        "ADC_Read_Task", // Tên
        4096,            // Stack Size
        NULL,            // Tham số
        5,               // Priority
        NULL,            // Task Handle
        1                // <-- CORE ID: 1 là APP_CPU, 0 là PRO_CPU (Bluetooth đang dùng)
    );
}