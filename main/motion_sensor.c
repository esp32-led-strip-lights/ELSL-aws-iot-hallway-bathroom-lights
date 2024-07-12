#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "motion_sensor.h"

static const char *TAG = "MOTION_SENSOR";
extern QueueHandle_t motion_event_queue;  // Declare the queue handle

static void IRAM_ATTR motion_sensor_isr_handler(void* arg) {
    // This function will be called when motion is detected
    uint32_t motion_detected = 1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(motion_event_queue, &motion_detected, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

void motion_sensor_init(void) {
    gpio_config_t io_conf;
    // Configure the PIR sensor pin as input
    io_conf.intr_type = GPIO_INTR_POSEDGE; // Trigger on rising edge
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << MOTION_SENSOR_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Install ISR service with default configuration
    gpio_install_isr_service(0);
    // Attach the interrupt service routine
    gpio_isr_handler_add(MOTION_SENSOR_PIN, motion_sensor_isr_handler, (void*) MOTION_SENSOR_PIN);

    ESP_LOGI(TAG, "PIR sensor initialized.");
}
