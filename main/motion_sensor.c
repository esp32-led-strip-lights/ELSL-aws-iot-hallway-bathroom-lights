#include "motion_sensor.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"  // Include semaphore header
#include "freertos/task.h"

#define DEBOUNCE_PERIOD_MS 1000

static const char* TAG = "MOTION_SENSOR";

extern QueueHandle_t motion_event_queue;  // Declare the queue handle

static volatile uint32_t last_motion_time = 0;
static volatile uint32_t debounce_end_time = 0;

static volatile bool log_event_queue_full = false;
static volatile bool log_debounce_event = false;
static volatile bool log_motion_detected = false;

static void IRAM_ATTR motion_sensor_isr_handler(void* arg) {
    uint32_t motion_detected = 1;
    uint32_t current_time = xTaskGetTickCountFromISR();

    // Software debounce logic
    if (current_time > debounce_end_time) {
        debounce_end_time = current_time + pdMS_TO_TICKS(DEBOUNCE_PERIOD_MS);
        last_motion_time = current_time;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        if (xQueueSendFromISR(motion_event_queue, &motion_detected, &xHigherPriorityTaskWoken) !=
            pdPASS) {
            log_event_queue_full = true;
        } else {
            log_motion_detected = true;
        }

    } else {
        log_debounce_event = true;
    }
}

void motion_logging_task(void* pvParameters) {
    while (1) {
        if (log_motion_detected) {
            ESP_LOGW(TAG, "Motion detected!");
            log_motion_detected = false;
        }
        if (log_event_queue_full) {
            ESP_LOGW(TAG, "Motion event queue full, event lost.");
            log_event_queue_full = false;
        }
        if (log_debounce_event) {
            ESP_LOGI(TAG, "Motion event debounced.");
            log_debounce_event = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // Adjust the delay as needed
    }
}

void motion_sensor_init(void) {
    gpio_config_t io_conf = {.intr_type = GPIO_INTR_POSEDGE,  // Trigger on rising edge
                             .mode = GPIO_MODE_INPUT,         // Set as input mode
                             .pin_bit_mask = (1ULL << MOTION_SENSOR_PIN),
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_DISABLE};

    gpio_config(&io_conf);

    // Install ISR service with default configuration
    gpio_install_isr_service(0);
    // Attach the interrupt service routine
    gpio_isr_handler_add(MOTION_SENSOR_PIN, motion_sensor_isr_handler, NULL);

    ESP_LOGI(TAG, "PIR sensor initialized.");

    // Create the logging task
    xTaskCreate(motion_logging_task, "motion_logging_task", 2048, NULL, 10, NULL);
}
