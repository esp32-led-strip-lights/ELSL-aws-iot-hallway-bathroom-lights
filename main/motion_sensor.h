#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Define the PIR sensor GPIO pin
#define MOTION_SENSOR_PIN 18

// Function to initialize the PIR motion sensor
void motion_sensor_init(void);

#endif // MOTION_SENSOR_H
