#ifndef __GPS_H__
#define __GPS_H__

#include "stm32f1xx_hal.h"
#include "dht11.h"
#include "loop_buffer.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int utc_hour;           //UTC小时
    int utc_minute;         //UTC分钟
    int utc_second;         //UTC秒
    int beijing_hour;       //北京时间小时
    double latitude;        //纬度
    int latitude_degree;    //纬度度
    int latitude_minute;    //纬度分
    int latitude_second;    //纬度秒
    double longitude;       //经度
    int longitude_degree;   //经度度
    int longitude_minute;   //经度分
    int longitude_second;   //经度秒
    char ns;                //纬度半球
    char ew;                //经度半球
    uint8_t fix_valid;      //定位有效性
} GPS_Data_t;


void parse_simple(note_t *note,GPS_Data_t *gps_data);

#endif 

