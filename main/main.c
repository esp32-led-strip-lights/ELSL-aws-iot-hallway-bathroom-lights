#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "motion_sensor.h"
#include "wifi.h"
#include "mqtt.h"
#include "led_strip.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "led_handler.h"
#include "cJSON.h"
#include "esp_insights.h"
#include "esp_system.h"
#include "insights.h"
#include "ota.h"

static const char *TAG = "MAIN";

static EventGroupHandle_t wifi_event_group;
QueueHandle_t motion_event_queue;
SemaphoreHandle_t wifi_connected_semaphore;  // Define the semaphore handle
SemaphoreHandle_t motion_detection_semaphore; // Semaphore for motion detection

const int WIFI_CONNECTED_BIT = BIT0;

void motion_detection_task(void *pvParameter) {
    uint32_t motion_event;
    while (1) {
        if (xQueueReceive(motion_event_queue, &motion_event, portMAX_DELAY)) {
            uint32_t led_event = LED_ON;
            xQueueSend(led_event_queue, &led_event, portMAX_DELAY);
        }
    }
}

void wifi_management_task(void *pvParameter) {
    wifi_init_sta();
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to WiFi");
            xSemaphoreGive(wifi_connected_semaphore); // Give the semaphore when WiFi is connected
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void mqtt_handling_task(void *pvParameter) {
    // Wait for the WiFi to be connected
    if (xSemaphoreTake(wifi_connected_semaphore, portMAX_DELAY)) {
        mqtt_app_start();
        while (1) {
            // Handle MQTT events
            if (!mqtt_client_is_connected()) {
                ESP_LOGI(TAG, "MQTT Disconnected!");
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}


void app_main(void) {
    char buffer[128];
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create event group for WiFi
    wifi_event_group = xEventGroupCreate();
    // Create queue for motion detection events
    motion_event_queue = xQueueCreate(10, sizeof(uint32_t));
    // Create the semaphore
    wifi_connected_semaphore = xSemaphoreCreateBinary();
    motion_detection_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(motion_detection_semaphore); // Start with the semaphore given

    wifi_init_sta(); 

    init_insights();

    mqtt_app_start(); // Initialize MQTT without expecting a return value
    esp_mqtt_client_handle_t mqtt_client = mqtt_get_client(); // Get the client handle

    led_handler_init();

    motion_sensor_init();

    // Create tasks
    xTaskCreate(&motion_detection_task, "motion_detection_task", 2048, NULL, 5, NULL);
    xTaskCreate(&wifi_management_task, "wifi_management_task", 4096, NULL, 6, NULL);
    xTaskCreate(&mqtt_handling_task, "mqtt_handling_task", 8192, NULL, 5, NULL);
    xTaskCreate(&led_handling_task, "led_handling_task", 8192, NULL, 5, NULL);
    
    if (was_booted_after_ota_update()) {
        ESP_LOGI(TAG, "Device booted after an OTA update.");
        const esp_mqtt_client_handle_t current_mqtt_client = mqtt_get_client();
        cJSON *root = cJSON_CreateObject();
        sprintf(buffer, "Successful reboot after OTA update");
        cJSON_AddStringToObject(root, CONFIG_WIFI_HOSTNAME, buffer);
        const char *json_string = cJSON_Print(root);
        esp_mqtt_client_publish(current_mqtt_client, CONFIG_MQTT_PUBLISH_OTA_PROGRESS_TOPIC, json_string, 0, 1, 0);
        free(root);
        free(json_string);
    } else {
        ESP_LOGI(TAG, "Device did not boot after an OTA update.");
    }

    // Infinite loop to prevent exiting app_main
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
