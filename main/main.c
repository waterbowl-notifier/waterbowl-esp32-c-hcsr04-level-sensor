#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "hcsr04.h"

static const char *TAG = "WATER_BOWL";

void app_main()
{
    hcsr04_init();

    while (true)
    {
        ESP_LOGI(TAG, "Distance: %.1f cm", hcsr04_get_range());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
