#ifndef __DHT11_H__
#define __DHT11_H__

#include "stm32f1xx_hal.h"
#include "main.h"
#include "delay.h"
#include <stdio.h>

int DHT11_Read_Data(float *humidity,float *temperature);


#endif  /*dht11.h*/
