#ifndef MQTT_H
#define MQTT_H

#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern esp_mqtt_client_handle_t mqtt_client; // Declare mqtt_client as extern

void mqtt_app_start(void);
esp_mqtt_client_handle_t mqtt_get_client(void);
int mqtt_client_is_connected(void);

#endif // MQTT_H
