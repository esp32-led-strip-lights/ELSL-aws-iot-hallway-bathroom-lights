#ifndef INSIGHTS_H
#define INSIGHTS_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_insights.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INSIGHTS_EVENT_QUEUE_SIZE 10

typedef struct {
    esp_event_base_t event_base;
    int32_t event_id;
    void *event_data;
} insights_event_t;

void insights_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void insights_task(void *arg);

void init_insights(void);

#ifdef __cplusplus
}
#endif

#endif // INSIGHTS_H
