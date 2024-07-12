#include "esp_err.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "led_strip.h"

#define BLINK_GPIO 2

led_strip_handle_t led_strip;

/* LED strip initialization with the GPIO and pixels number */
led_strip_config_t strip_config = {
    .strip_gpio_num = BLINK_GPIO, // The GPIO that is connected to the LED strip's data line
    .max_leds = 10, // The number of LEDs in the strip
    .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
    .led_model = LED_MODEL_WS2812, // LED strip model
    .flags.invert_out = false, // Whether to invert the output signal (useful when your hardware has a level inverter)
};

led_strip_rmt_config_t led_strip_rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    .rmt_channel = 0,
#else
    .clk_src = RMT_CLK_SRC_DEFAULT, // Different clock sources can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
    .flags.with_dma = false, // Whether to enable the DMA feature
#endif
};

void app_main(void) {
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &led_strip_rmt_config, &led_strip);
    ESP_ERROR_CHECK(ret);
    // Additional code for your application
}
