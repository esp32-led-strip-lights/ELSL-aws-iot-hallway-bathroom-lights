#ifndef WIFI_H
#define WIFI_H
#include <stdbool.h>  

#define MAX_RETRY 7 

void wifi_init_sta(void);
extern bool wifi_is_connected;

#endif // WIFI_H
