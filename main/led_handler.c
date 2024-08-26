#include "led_handler.h"

#include <inttypes.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "LED_HANDLER";

#define BRIGHTNESS 0x606060

led_strip_handle_t led_strip;

/* LED strip initialization with the GPIO and pixels number */
led_strip_config_t strip_config = {
    .strip_gpio_num = CONFIG_LED_STRIP_GPIO_PIN, // The GPIO that is connected to the LED strip's data line
    .max_leds = CONFIG_MAX_LED_COUNT,            // This will be set in led_handler_init()
    .led_pixel_format = LED_PIXEL_FORMAT_GRB,    // Pixel format of your LED strip
    .led_model = LED_MODEL_WS2812,               // LED strip model
    .flags.invert_out = false,                   // Whether to invert the output signal (useful when your hardware
                                                 // has a level inverter)
};

led_strip_rmt_config_t led_strip_rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,    // Different clock sources can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
    .flags.with_dma = false,           // Whether to enable the DMA feature
};

QueueHandle_t led_state_queue;

QueueHandle_t get_led_state_queue(void) { return led_state_queue; }

// Function to convert milliseconds to minutes
uint32_t ms_to_minutes(uint32_t milliseconds) { return milliseconds / (60 * 1000); }

void init_led_handler()
{
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &led_strip_rmt_config, &led_strip);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing LED strip: %s", esp_err_to_name(ret));
        return;
    }

    // Create queue for LED handling events
    led_state_queue = xQueueCreate(10, sizeof(uint32_t));

    ESP_LOGI(TAG, "Initializing LED handler with %" PRIu32 " LEDs", strip_config.max_leds);
    // Turn off the LED strip initially
    ret = led_strip_clear(led_strip);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error clearing LED strip: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "LED strip cleared at initialization.");
    }
    xTaskCreate(&led_handling_task, "led_handling_task", 8192, NULL, 5, NULL);
}

void light_led_strip_from_center_out(uint32_t color)
{
    int center = strip_config.max_leds / 2;
    for (int i = 0; i <= center; i++)
    {
        if (center + i < strip_config.max_leds)
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, center + i, (color >> 16) & 0xFF,
                                                (color >> 8) & 0xFF, color & 0xFF));
        }
        if (center - i >= 0)
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, center - i, (color >> 16) & 0xFF,
                                                (color >> 8) & 0xFF, color & 0xFF));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(pdMS_TO_TICKS(CONFIG_PAUSE_BETWEEN_LEDS_MS)); // Adjust delay for desired speed
    }
}

void set_all_leds_off()
{
    for (int i = 0; i < strip_config.max_leds; i++)
    {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 0, 0, 0));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_handling_task(void *pvParameter)
{
    uint32_t led_event;
    bool led_on = false;

    assert(led_state_queue != NULL);
    led_strip_clear(led_strip);

    while (1)
    {
        if (xQueueReceive(led_state_queue, &led_event, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Received event: %" PRIu32, led_event);
            if (led_event && !led_on)
            {
                led_on = true;
                ESP_LOGI(TAG, "Turning on the LED strip.");
                // Light the LED strip from center outwards
                light_led_strip_from_center_out(BRIGHTNESS);

                // Delay based on CONFIG_MAX_LED_SHINE_MINUTES
                uint32_t shine_duration = CONFIG_MAX_LED_SHINE_MINUTES * 60 * 1000;
                uint32_t shine_duration_minutes = ms_to_minutes(shine_duration);
                ESP_LOGI(TAG, "LEDs will shine for %" PRIu32 " minutes (%" PRIu32 " milliseconds).",
                         shine_duration_minutes, shine_duration);

                vTaskDelay(pdMS_TO_TICKS(shine_duration));

                ESP_LOGI(TAG, "Turning off the LED strip.");

                // Set all LEDs to off before clearing
                set_all_leds_off();

                led_on = false;
            }
            else if (led_on)
            {
                ESP_LOGI(TAG, "No-Op: LED strip is already on.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay at the bottom of the loop
    }
}
