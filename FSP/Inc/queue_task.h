#ifndef __QUEUE_TASK_H__
#define __QUEUE_TASK_H__

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "event_groups.h"
#include "oled.h"
#include "semphr.h"
#include "dht11.h"
#include "gps.h"

typedef struct
{
    float temperature;
    float humidity;
} DHT11_Data_t;

extern TaskHandle_t gpstaskHandle;

void GPSProject_Create(void);

#endif
