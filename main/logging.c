#include "logging.h"
#include "mqtt.h"
#include "esp_log.h"
#include "cJSON.h"

QueueHandle_t log_queue; // Define log_queue

int mqtt_vprintf(const char *fmt, va_list args) { // Define mqtt_vprintf
    char log_buffer[LOG_MESSAGE_MAX_LENGTH];
    int len = vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);
    if (len >= 0 && len < LOG_MESSAGE_MAX_LENGTH) {
        log_message_t log_message;
        strncpy(log_message.message, log_buffer, LOG_MESSAGE_MAX_LENGTH - 1);
        log_message.message[LOG_MESSAGE_MAX_LENGTH - 1] = '\0';
        if (xQueueSend(log_queue, &log_message, portMAX_DELAY) != pdPASS) {
            ESP_LOGE("MQTT_LOG", "Failed to send log message to queue");
        }
    }
    return len;
}

void logging_task(void *pvParameters) {
    log_message_t log_message;
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_message_t)); // Initialize log_queue
    if (log_queue == NULL) {
        ESP_LOGE("LOGGING", "Failed to create log queue");
        return;
    }
    esp_log_set_vprintf(mqtt_vprintf); // Set custom vprintf function
    ESP_LOGI("LOGGING", "Custom logging initialized. Host: %s Topic: %s", CONFIG_WIFI_HOSTNAME, CONFIG_MQTT_PUBLISH_LOGGING_TOPIC);

    esp_mqtt_client_handle_t mqtt_client = (esp_mqtt_client_handle_t)pvParameters;
    while (true) {
        if (xQueueReceive(log_queue, &log_message, portMAX_DELAY) == pdPASS) {
            if (mqtt_client != NULL) {
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, CONFIG_WIFI_HOSTNAME, log_message.message);
                const char *json_string = cJSON_Print(root);
                esp_mqtt_client_publish(mqtt_client, CONFIG_MQTT_PUBLISH_LOGGING_TOPIC, json_string, 0, 1, 0);
                cJSON_Delete(root);
            }
        }
    }
}
