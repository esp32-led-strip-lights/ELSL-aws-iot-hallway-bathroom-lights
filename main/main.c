#include "cJSON.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gecl-heartbeat-manager.h"
#include "gecl-logger-manager.h"
#include "gecl-misc-util-manager.h"
#include "gecl-mqtt-manager.h"
#include "gecl-ota-manager.h"
#include "gecl-telemetry-manager.h"
#include "gecl-time-sync-manager.h"
#include "gecl-versioning-manager.h"
#include "gecl-wifi-manager.h"
#include "led_handler.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "MAIN";

QueueHandle_t motion_event_queue;
QueueHandle_t log_queue = NULL;
QueueHandle_t led_state_queue = NULL;

extern const uint8_t home_hallway_bathroom_lights_certificate_pem[];
extern const uint8_t home_hallway_bathroom_lights_private_pem_key[];

void custom_handle_mqtt_event_connected(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t client = event->client;
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_CONNECTED");

    ESP_LOGI(TAG, "Subscribing to topic %s", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_CONTROLLER_TOPIC);
    esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_CONTROLLER_TOPIC, 0);

    ESP_LOGI(TAG, "Subscribing to topic %s", CONFIG_MQTT_SUBSCRIBE_TELEMETRY_REQUEST_TOPIC);
    esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_TELEMETRY_REQUEST_TOPIC, 0);

    if (read_sensors_task_handle == NULL) {
        xTaskCreate(&read_sensors_task, "read_sensors_task", 4096, (void *)client, 5,
                    &read_sensors_task_handle);
    }
}

void custom_handle_mqtt_event_disconnected(esp_mqtt_event_handle_t event) {
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_DISCONNECTED");
    if (ota_handler_task_handle != NULL) {
        vTaskDelete(ota_handler_task_handle);
        ota_handler_task_handle = NULL;
    }
}

void custom_handle_mqtt_event_data(esp_mqtt_event_handle_t event) {
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_DATA");
    if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_CONTROLLER_TOPIC,
                event->topic_len) == 0) {
        ESP_LOGI(TAG, "Received topic %s", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_CONTROLLER_TOPIC);
        if (ota_handler_task_handle != NULL) {
            eTaskState task_state = eTaskGetState(ota_handler_task_handle);
            if (task_state != eDeleted) {
                ESP_LOGW(TAG,
                         "OTA task is already running or not yet cleaned up, skipping OTA update");
                return;
            }
            // Clean up task handle if it has been deleted
            ota_handler_task_handle = NULL;
        }
        xTaskCreate(&ota_handler_task, "ota_handler_task", 8192, event, 5,
                    &ota_handler_task_handle);
    } else if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_TELEMETRY_REQUEST_TOPIC,
                       event->topic_len) == 0) {
        ESP_LOGI(TAG, "Received topic %s", CONFIG_MQTT_SUBSCRIBE_TELEMETRY_REQUEST_TOPIC);
        transmit_telemetry();
    }
}

void custom_handle_mqtt_event_error(esp_mqtt_event_handle_t event) {
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
        ESP_LOGI(TAG, "Last ESP error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGI(TAG, "Last TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGI(TAG, "Last TLS library error code: 0x%x",
                 event->error_handle->esp_tls_cert_verify_flags);
    } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    } else {
        ESP_LOGI(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
    esp_restart();
}

QueueHandle_t start_logging(void) {
    log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_message_t));

    if (log_queue == NULL) {
        ESP_LOGE("MISC_UTIL", "Failed to create logger queue");
        esp_restart();
    }

    xTaskCreate(&logger_task, "logger_task", 4096, NULL, 5, NULL);
    return log_queue;
}

void setup_nvs_flash(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

esp_mqtt_client_handle_t start_mqtt(const mqtt_config_t *config) {
    // Define the configuration

    // Set the custom event handlers
    mqtt_set_event_connected_handler(custom_handle_mqtt_event_connected);
    mqtt_set_event_disconnected_handler(custom_handle_mqtt_event_disconnected);
    mqtt_set_event_data_handler(custom_handle_mqtt_event_data);
    mqtt_set_event_error_handler(custom_handle_mqtt_event_error);

    // Start the MQTT client
    esp_mqtt_client_handle_t client = mqtt_app_start(config);

    return client;
}

void app_main(void) {
    setup_nvs_flash();

    wifi_init_sta();

    synchronize_time();

    log_queue = start_logging();

    mqtt_config_t config = {.certificate = home_hallway_bathroom_lights_certificate_pem,
                            .private_key = home_hallway_bathroom_lights_private_pem_key,
                            .broker_uri = CONFIG_AWS_IOT_ENDPOINT};

    esp_mqtt_client_handle_t client = start_mqtt(&config);

    xTaskCreate(&heartbeat_task, "heartbeat_task", 4096, (void *)client, 5, NULL);

    xTaskCreate(&led_handling_task, "led_handling_task", 8192, NULL, 5, NULL);

    init_telemetry_manager(device_name, client, CONFIG_MQTT_PUBLISH_TELEMETRY_TOPIC);

    transmit_telemetry();

    // Infinite loop to prevent exiting app_main
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Delay to allow other tasks to run
    }
}

#if 0
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

    wifi_init_sta();

    synchronize_time();

    // init_insights();

    mqtt_app_start();  // Initialize MQTT without expecting a return value
    esp_mqtt_client_handle_t mqtt_client = mqtt_get_client();  // Get the client handle

    led_handler_init();

    motion_sensor_init();

    // Create tasks
    xTaskCreate(&motion_detection_task, "motion_detection_task", 2048, NULL, 5, NULL);
    xTaskCreate(&wifi_management_task, "wifi_management_task", 4096, NULL, 6, NULL);
    xTaskCreate(&mqtt_handling_task, "mqtt_handling_task", 8192, NULL, 5, NULL);
    xTaskCreate(&led_handling_task, "led_handling_task", 8192, NULL, 5, NULL);

    // Infinite loop to prevent exiting app_main
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

#if 0
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIR_PIN 13
// static const char *TAG = "PIR_TEST";

void app_main() {
    gpio_config_t io_conf = {.intr_type = GPIO_INTR_DISABLE,  // No interrupt for testing
                             .mode = GPIO_MODE_INPUT,
                             .pin_bit_mask = (1ULL << PIR_PIN),
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_DISABLE};
    gpio_config(&io_conf);

    while (1) {
        int pir_value = gpio_get_level(PIR_PIN);
        ESP_LOGI(TAG, "PIR sensor value: %d", pir_value);
        vTaskDelay(pdMS_TO_TICKS(100));  // Check the sensor every 100ms
    }
}
#endif