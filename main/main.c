#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mbedtls/debug.h"
#include "wifi.h"
#include "mqtt.h"
#include "sdkconfig.h"
#include "motion_sensor.h"


static const char *TAG = "MAIN";

static void tls_debug_callback(void *ctx, int level, const char *file, int line, const char *str)
{
    // Uncomment to enable verbose debugging
    // const char *MBEDTLS_DEBUG_LEVEL[] = {"Error", "Warning", "Info", "Debug", "Verbose"};
    // ESP_LOGI("mbedTLS", "%s: %s:%04d: %s", MBEDTLS_DEBUG_LEVEL[level], file, line, str);
}

void app_main(void)
{
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_DEBUG);
    esp_log_level_set("esp-tls", ESP_LOG_DEBUG);
    esp_log_level_set("transport", ESP_LOG_DEBUG);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG,"Initialize motion sensor");
    motion_sensor_init();

    ESP_LOGI(TAG,"Initialize WiFi");
    wifi_init_sta();

    ESP_LOGI(TAG,"Initialize MQTT");
    mqtt_app_start();

     // Infinite loop to prevent exiting app_main
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow other tasks to run
    }
}
