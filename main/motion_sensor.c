#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "motion_sensor.h"

#define DEBOUNCE_PERIOD_MS 1000

static const char *TAG = "MOTION_SENSOR";

extern QueueHandle_t motion_event_queue;  // Declare the queue handle

static volatile uint32_t last_motion_time = 0;
static volatile uint32_t debounce_end_time = 0;

static void IRAM_ATTR motion_sensor_isr_handler(void* arg) {
    uint32_t motion_detected = 1;
    uint32_t current_time = xTaskGetTickCountFromISR();

    // Software debounce logic
    if (current_time > debounce_end_time) {
        debounce_end_time = current_time + pdMS_TO_TICKS(DEBOUNCE_PERIOD_MS);  
        last_motion_time = current_time;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xQueueSendFromISR(motion_event_queue, &motion_detected, &xHigherPriorityTaskWoken) != pdPASS) {
            ESP_LOGW(TAG, "Motion event queue full, event lost.");
        }
        if (xHigherPriorityTaskWoken != pdFALSE) {
            portYIELD_FROM_ISR();
        }
    } else {
        ESP_LOGI(TAG, "Motion event debounced.");
    }
}

void motion_sensor_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,          // Trigger on rising edge
        .mode = GPIO_MODE_INPUT,                 // Set as input mode
        .pin_bit_mask = (1ULL << MOTION_SENSOR_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,   // Disable pull-down
        .pull_up_en = GPIO_PULLUP_ENABLE         // Enable pull-up if the sensor has open-drain output
    };

    gpio_config(&io_conf);

    // NOTE: Add a 0.1µF capacitor between VCC (3.3V) and GND near the sensor
    // to filter out high-frequency noise and stabilize the power supply.
    // Also consider adding a larger capacitor (10µF or 100µF) in parallel.

    // Install ISR service with default configuration
    gpio_install_isr_service(0);
    // Attach the interrupt service routine
    gpio_isr_handler_add(MOTION_SENSOR_PIN, motion_sensor_isr_handler, NULL);

    ESP_LOGI(TAG, "PIR sensor initialized.");
}
