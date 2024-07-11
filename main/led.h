#ifndef LED_H
#define LED_H

#include "sdkconfig.h"
#include <stdint.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h" 

#define RED_PIN GPIO_NUM_25
#define GREEN_PIN GPIO_NUM_26
#define BLUE_PIN GPIO_NUM_27

typedef enum {
    LED_OFF,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_FLASHING_RED,
    LED_FLASHING_GREEN,
    LED_FLASHING_BLUE,
    LED_FLASHING_WHITE,
    LED_PULSATING_RED,
    LED_PULSATING_GREEN,
    LED_PULSATING_BLUE,
    LED_PULSATING_WHITE
} led_state_t;

extern volatile led_state_t current_led_state;

void init_led_pwm(void);
void set_led_color(uint32_t red, uint32_t green, uint32_t blue);
void led_task(void *pvParameter); 

#endif // LED_H
