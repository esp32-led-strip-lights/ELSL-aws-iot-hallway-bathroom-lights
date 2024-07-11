#ifndef SENSORS_H
#define SENSORS_H

#define LEFT_SENSOR_GPIO GPIO_NUM_4
#define RIGHT_SENSOR_GPIO GPIO_NUM_15

typedef enum {
    DOOR_STATUS_OPEN,
    DOOR_STATUS_CLOSED,
    DOOR_STATUS_ERROR,
    DOOR_STATUS_UNKNOWN
} door_status_t;

void read_sensors_task(void *pvParameters);
void init_sensors_gpio(void);
int read_sensor_data(void);

#endif

