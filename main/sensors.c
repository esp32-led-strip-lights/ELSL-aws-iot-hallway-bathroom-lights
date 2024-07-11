#include "sensors.h"
#include "led.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "mqtt.h" 
#include "esp_timer.h"
#include "sdkconfig.h"


static const char *TAG = "SENSORS";

#define TRANSMIT_STATUS_INTERVAL_IN_MINUTES ( CONFIG_STATUS_TRANSMIT_INTERVAL * 60 * 1000)

#define MAIN_LOOP_SLEEP_MS 2000
#define STATUS_UPDATE_INTERVAL_MS 60000

extern bool is_mqtt_connected;

door_status_t current_door_status = DOOR_STATUS_UNKNOWN;
door_status_t last_door_status = DOOR_STATUS_UNKNOWN;

static uint64_t last_publish_time = 0;

void init_sensors_gpio() {
    gpio_config_t io_conf;

    // Configure GPIO for sensors
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << LEFT_SENSOR_GPIO) | (1ULL << RIGHT_SENSOR_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}

static void publish_door_status(esp_mqtt_client_handle_t client, door_status_t status) {
    const char *status_str = NULL;
    switch (status) {
        case DOOR_STATUS_OPEN:
            status_str = "OPEN";
            break;
        case DOOR_STATUS_CLOSED:
            status_str = "CLOSED";
            break;
        case DOOR_STATUS_ERROR:
            status_str = "ERROR";
            break;
        default:
            status_str = "UNKNOWN";
            break;
    }

    if (!is_mqtt_connected) {
        ESP_LOGE(TAG, "MQTT client is not connected, cannot publish door status");
        return;
    }

    ESP_LOGI(TAG, "Publishing door status: %s", status_str);
    char message[50];
    snprintf(message, sizeof(message), "{\"door\":\"%s\"}", status_str);
    int msg_id = esp_mqtt_client_publish(client, CONFIG_MQTT_PUBLISH_STATUS_TOPIC, message, 0, 1, 0);
    ESP_LOGI(TAG, "publish successful to %s, msg_id=%d", CONFIG_MQTT_PUBLISH_STATUS_TOPIC, msg_id);

    // Update the last publish time
    last_publish_time = esp_timer_get_time() / 1000;
}

void read_sensors_task(void *pvParameters) {
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;

    while (true) {
        int left_sensor_value = gpio_get_level(LEFT_SENSOR_GPIO);
        int right_sensor_value = gpio_get_level(RIGHT_SENSOR_GPIO);

        if (left_sensor_value == right_sensor_value) {
            current_door_status = left_sensor_value ? DOOR_STATUS_CLOSED : DOOR_STATUS_OPEN;
        } else {
            ESP_LOGE(TAG, "Left and right sensor values do not match!");
            current_door_status = DOOR_STATUS_ERROR;
        }

        uint64_t current_time = esp_timer_get_time() / 1000;
        if (current_door_status != last_door_status || 
            (current_time - last_publish_time) >= TRANSMIT_STATUS_INTERVAL_IN_MINUTES) {
            publish_door_status(client, current_door_status);
            last_door_status = current_door_status;
        }

        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_SLEEP_MS));
    }
}
