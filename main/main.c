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
#include "gecl-motion-sensor-manager.h"
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
const char *device_name = CONFIG_WIFI_HOSTNAME;

TaskHandle_t ota_handler_task_handle = NULL;

extern const uint8_t home_hallway_bathroom_lights_certificate_pem[];
extern const uint8_t home_hallway_bathroom_lights_private_pem_key[];

#define QUEUE_SIZE 10
typedef struct {
    int motion_detected;
} motion_event_t;

typedef struct {
    int led_action;
} led_event_t;

typedef struct {
    QueueHandle_t motion_queue;
    QueueHandle_t led_queue;
} motion_led_params_t;

void custom_handle_mqtt_event_connected(esp_mqtt_event_handle_t event) {
    esp_mqtt_client_handle_t client = event->client;
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_CONNECTED");

    ESP_LOGI(TAG, "Subscribing to topic %s", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_TOPIC);
    esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_TOPIC, 0);

    ESP_LOGI(TAG, "Subscribing to topic %s", CONFIG_MQTT_SUBSCRIBE_TELEMETRY_REQUEST_TOPIC);
    esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_TELEMETRY_REQUEST_TOPIC, 0);
}

void custom_handle_mqtt_event_disconnected(esp_mqtt_event_handle_t event) {
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_DISCONNECTED");
}

void custom_handle_mqtt_event_data(esp_mqtt_event_handle_t event) {
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_DATA");
    if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_TOPIC, event->topic_len) == 0) {
        ESP_LOGI(TAG, "Received topic %s", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_TOPIC);
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

void pass_motion_to_led_task(void *pvParameter) {
    motion_led_params_t *params = (motion_led_params_t *)pvParameter;
    motion_event_t motion_event;
    led_event_t led_event;
    assert(params->motion_queue != NULL && params->led_queue != NULL);

    while (1) {
        if (xQueueReceive(params->motion_queue, &motion_event, portMAX_DELAY)) {
            led_event.led_action = motion_event.motion_detected;
            xQueueSend(params->led_queue, &led_event, 0);
            vTaskDelay(pdMS_TO_TICKS(100));  // Delay to allow other tasks to run
        }
    }
}

void associate_led_with_motion() {
    QueueHandle_t motion_queue = get_motion_event_queue();
    QueueHandle_t led_queue = get_led_state_queue();

    if (motion_queue == NULL || led_queue == NULL) {
        printf("Failed to create queues\n");
        return;
    }

    // Allocate and initialize task parameters
    motion_led_params_t *params = (motion_led_params_t *)malloc(sizeof(motion_led_params_t));
    if (params == NULL) {
        // Handle error
        printf("Failed to allocate memory for task parameters\n");
        return;
    }
    params->motion_queue = motion_queue;
    params->led_queue = led_queue;

    // Create the task and pass the parameters
    xTaskCreate(pass_motion_to_led_task, "pass_motion_to_led_task", 2048, (void *)params, 5, NULL);
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

    init_heartbeat_manager(client, CONFIG_MQTT_PUBLISH_HEARTBEAT_TOPIC,
                           CONFIG_MQTT_HEARTBEAT_INTERVAL_MINUTES);

    init_telemetry_manager(device_name, client, CONFIG_MQTT_PUBLISH_TELEMETRY_TOPIC,
                           CONFIG_MQTT_TELEMETRY_INTERVAL_MINUTES);

    init_motion_sensor_manager();

    init_led_handler();

    associate_led_with_motion();

    // Infinite loop to prevent exiting app_main
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Delay to allow other tasks to run
    }
}