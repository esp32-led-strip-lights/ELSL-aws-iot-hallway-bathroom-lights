#include <stdio.h>
#include <inttypes.h>  // For PRId32 macro
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_insights.h"
#include "esp_event.h"
#include "insights.h"

#define INSIGHTS_EVENT_QUEUE_SIZE 10

extern const char esp_insights_auth_key[];

static const char *TAG = "INSIGHTS";

static QueueHandle_t insights_event_queue;

void insights_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    insights_event_t evt;
    evt.event_base = event_base;
    evt.event_id = event_id;
    evt.event_data = event_data;

    if (xQueueSend(insights_event_queue, &evt, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send event to queue");
    }
}

void insights_task(void *arg) {
    insights_event_t evt;

    while (true) {
        if (xQueueReceive(insights_event_queue, &evt, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Handling event %" PRId32, evt.event_id);  // Use PRId32 for int32_t
            // Process the event
        }
    }
}

void init_insights() {
    esp_err_t err;

    insights_event_queue = xQueueCreate(INSIGHTS_EVENT_QUEUE_SIZE, sizeof(insights_event_t));
    if (insights_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    esp_insights_config_t config  = {
        .log_type = ESP_DIAG_LOG_TYPE_ERROR,
        .auth_key = esp_insights_auth_key,
    };

    err = esp_insights_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP Insights: %s", esp_err_to_name(err));
        return;
    }

    // Ensure the Insights system is initialized before registering the event handler
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(err));
        return;
    }

    err = esp_event_handler_register(INSIGHTS_EVENT, ESP_EVENT_ANY_ID, insights_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(err));
        return;
    }

    err = esp_insights_enable(&config);  // Pass the config structure
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ESP Insights: %s", esp_err_to_name(err));
    }

    // Create a task to process insights events
    xTaskCreate(insights_task, "insights_task", 4096, NULL, 5, NULL);
}
