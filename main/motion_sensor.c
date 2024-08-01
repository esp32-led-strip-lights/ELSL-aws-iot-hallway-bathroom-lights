#include "motion_sensor.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "MOTION_SENSOR";

// Debounce time in milliseconds
#define DEBOUNCE_TIME_MS 2000

static void motion_sensor_task(void* pvParameters) {
    bool motion_detected = false;
    TickType_t last_detection_time = 0;

    while (1) {
        int current_state = gpio_get_level(MOTION_SENSOR_PIN);
        TickType_t current_time = xTaskGetTickCount();

        if (current_state && !motion_detected &&
            (current_time - last_detection_time) > pdMS_TO_TICKS(DEBOUNCE_TIME_MS)) {
            ESP_LOGI(TAG, "Motion detected!");
            motion_detected = true;
            last_detection_time = current_time;
            // Add your processing logic here
        } else if (!current_state) {
            motion_detected = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // Adjust the delay as necessary
    }
}

void motion_sensor_init(void) {
    gpio_config_t io_conf = {.intr_type = GPIO_INTR_DISABLE,
                             .mode = GPIO_MODE_INPUT,
                             .pin_bit_mask = (1ULL << MOTION_SENSOR_PIN),
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_DISABLE};

    gpio_config(&io_conf);

    ESP_LOGI(TAG, "PIR sensor initialized.");

    // Create the motion sensor task
    xTaskCreate(motion_sensor_task, "motion_sensor_task", 2048, NULL, 10, NULL);
}
