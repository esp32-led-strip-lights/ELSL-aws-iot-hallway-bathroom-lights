#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define LED_ON 1
#define LED_OFF 0

extern QueueHandle_t led_event_queue;

void led_handler_init();
void led_handling_task(void *pvParameter);

#endif // LED_HANDLER_H
