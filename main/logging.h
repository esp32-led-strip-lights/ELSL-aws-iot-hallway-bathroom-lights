#ifndef LOGGING_H
#define LOGGING_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "mqtt_client.h"

#define LOG_QUEUE_SIZE 10
#define LOG_MESSAGE_MAX_LENGTH 1024

typedef struct {
    char message[LOG_MESSAGE_MAX_LENGTH];
} log_message_t;

extern QueueHandle_t log_queue; // Declare log_queue as extern
extern int mqtt_vprintf(const char *fmt, va_list args); // Declare mqtt_vprintf

void logging_task(void *pvParameters);

#endif // LOGGING_H
