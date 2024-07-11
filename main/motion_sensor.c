#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "motion_sensor.h"

static const char *TAG = "MOTION_SENSOR";

static void IRAM_ATTR motion_sensor_isr_handler(void* arg) {
    // This function will be called when motion is detected
    ESP_LOGI(TAG, "Motion detected!");
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
