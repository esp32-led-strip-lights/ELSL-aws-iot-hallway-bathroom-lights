#include "mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "mbedtls/debug.h"  // Add this to include mbedtls debug functions
#include "esp_random.h"
#include "ota.h"
#include "mqtt.h"
#include "sdkconfig.h"
#include "logging.h"

// Declare the global/static variables
bool mqtt_setup_complete = false;
bool mqtt_message_received = false;

TaskHandle_t read_sensors_task_handle = NULL;  // Task handle for read_sensors_task
TaskHandle_t ota_task_handle = NULL;  // Task handle for OTA updating
TaskHandle_t logging_task_handle = NULL;  // Task handle for console logging 

// Define NETWORK_TIMEOUT_MS
#define NETWORK_TIMEOUT_MS 10000  // Example value, set as appropriate for your application

extern const uint8_t home_hallway_bathroom_lights_certificate_pem[];
extern const uint8_t home_hallway_bathroom_lights_private_pem_key[];

const uint8_t *cert_start = home_hallway_bathroom_lights_certificate_pem;
const uint8_t *key_start = home_hallway_bathroom_lights_private_pem_key;

static const char *TAG = "MQTT";
bool is_mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_setup_complete = true; // MQTT setup is complete
        is_mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        is_mqtt_connected = false;
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG,"TOPIC=%.*s\r", event->topic_len, event->topic);
        ESP_LOGI(TAG,"DATA=%.*s\r", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
            ESP_LOGI(TAG, "Last ESP error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last TLS library error code: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGI(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    // Set mbedtls debug threshold to a higher level for detailed logs
    // mbedtls_debug_set_threshold(0);

    char client_id[32];
    snprintf(client_id, sizeof(client_id), "ESP32_%08" PRIx32, esp_random());

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = CONFIG_AWS_IOT_ENDPOINT,
            },
        },
        .credentials = {
            .client_id = client_id,
            .authentication = {
                .certificate = (const char *)cert_start,
                .key = (const char *)key_start,
            },
        },
        .network = {
            .timeout_ms = NETWORK_TIMEOUT_MS,
        },
        .session = {
            .keepalive = 120,
        },
        .buffer = {
            .size = 4096,
            .out_size = 4096,
        },
        .task = {
            .priority = 5,
        }
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;  // Do not proceed if initialization failed
    }

    ESP_LOGI(TAG, "MQTT client initialized successfully");

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);

    esp_err_t err;
    int retry_count = 0;
    const int max_retries = 5;
    const int retry_delay_ms = 5000;

    do {
        err = esp_mqtt_client_start(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start MQTT client, retrying in %d seconds... (%d/%d)", retry_delay_ms / 1000, retry_count + 1, max_retries);
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms)); // Delay for 5 seconds
            retry_count++;
        } else {
            ESP_LOGI(TAG, "MQTT client started successfully");
        }
    } while (err != ESP_OK && retry_count < max_retries);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client after %d retries", retry_count);
    }
}

void mqtt_subscribe_task(void *pvParameters)
{
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;

    while (1) {
        if (!mqtt_client_is_connected()) {
            ESP_LOGE(TAG, "MQTT client is not connected, cannot subscribe");
            vTaskDelay(pdMS_TO_TICKS(10000)); // Adjust delay as necessary
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // Adjust delay as necessary
    }
}

bool mqtt_client_is_connected(void) {
    return is_mqtt_connected;
}
