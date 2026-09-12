#include "queue_task.h"
#include "usart.h"
#include <stdarg.h>

note_t read_data;  // 接收循环缓冲区数据的临时变量
loop_buffer_t bd_uart_loop_buffer;

QueueHandle_t dht11Handle_t;
QueueHandle_t gpsQueueHandle;
SemaphoreHandle_t dht11ReadySem;
SemaphoreHandle_t printfMutex;
SemaphoreHandle_t oledMutex;
TaskHandle_t gpstaskHandle;

/* 线程安全的 printf：用互斥量串行化多任务对 USART1 的访问 */
void safe_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (printfMutex != NULL) xSemaphoreTake(printfMutex, portMAX_DELAY);
	vprintf(fmt, args);
	if (printfMutex != NULL) xSemaphoreGive(printfMutex);
	va_end(args);
}



void DHT11_Send_Task(void *arg)
{
    safe_printf("dht11 task is start\r\n");
    DHT11_Data_t dht11_data;
    BaseType_t ret;

    while(1)
    {
        if(DHT11_Read_Data(&dht11_data.humidity,&dht11_data.temperature) == 0)
		{
			ret = xQueueSend(dht11Handle_t,&dht11_data,portMAX_DELAY);
			if(ret == pdPASS)
            {
                safe_printf("Send：湿度：%.1f%%, 温度：%.1f℃\r\n", dht11_data.humidity, dht11_data.temperature);
				xSemaphoreGive(dht11ReadySem);// 通知OLED任务：新数据就绪
			}
            else
            {
                safe_printf("send data to queue fail\r\n");
            }
		}

        vTaskDelay(pdMS_TO_TICKS(2000)); //延时2s
    }
}

void OLED_Recv_Task(void *arg)
{
    safe_printf("oled task is start\r\n");
    DHT11_Data_t recv_data;
    GPS_Data_t gps_recv;
    BaseType_t ret;
    float last_humidity = 0, last_temperature = 0;
    int last_lat_d = 0, last_lat_m = 0, last_lat_s = 0;
    int last_lng_d = 0, last_lng_m = 0, last_lng_s = 0;

    while(1)
    {
        BaseType_t sem_ret = xSemaphoreTake(dht11ReadySem,pdMS_TO_TICKS(1000));
        if(sem_ret == pdTRUE)
        {
            ret = xQueueReceive(dht11Handle_t,&recv_data,0);
            if(ret == pdPASS)
            {
                last_humidity = recv_data.humidity;
                last_temperature = recv_data.temperature;
            }
        }

        ret = xQueueReceive(gpsQueueHandle,&gps_recv,0);
        if(ret == pdPASS)
        {
            last_lat_d = gps_recv.latitude_degree;
            last_lat_m = gps_recv.latitude_minute;
            last_lat_s = gps_recv.latitude_second;
            last_lng_d = gps_recv.longitude_degree;
            last_lng_m = gps_recv.longitude_minute;
            last_lng_s = gps_recv.longitude_second;
        }

        /* ===== OLED 显示：加互斥量保护 I2C 总线 ===== */
        xSemaphoreTake(oledMutex, portMAX_DELAY);

        /* 第0页：湿度  湿度(1-32):(34-41) 数值(43-74) %RH(75-98) */
        OLED_Show_Chinese(0,1,1);
        OLED_Show_Chinese(0,17,2);
        OLED_Show_Char(0,34,':');
		OLED_Show_Float(0,43,last_humidity,2,1);
		OLED_Show_String(0,75,"%RH");

		/* 第2页：温度  温度(1-32):(34-41) 数值(43-74) 度(75-90) */
		OLED_Show_Chinese(2,1,0);
        OLED_Show_Chinese(2,17,2);
        OLED_Show_Char(2,34,':');
		OLED_Show_Float(2,43,last_temperature,2,1);
		OLED_Show_Chinese(2,75,3);

		/* 第4页：纬度 lat=DD°MM'SS"N  (°为汉字16列宽) */
		OLED_Show_String(4,1,"lat=");			// col 1-32
		OLED_Show_Num(4,33,last_lat_d,2);		// col 33-48
		OLED_Show_Chinese(4,49,6);  // ° col 49-64
		OLED_Show_Num(4,65,last_lat_m,2);		// col 65-80
		OLED_Show_Char(4,81,'\'');				// col 81-88
		OLED_Show_Num(4,89,last_lat_s,2);		// col 89-104
		OLED_Show_Char(4,105,'"');				// col 105-112
		OLED_Show_Char(4,113,'N');				// col 113-120

		/* 第6页：经度 lng=DDD°MM'SS"E  (从col0开始以容纳128列) */
		OLED_Show_String(6,0,"lng=");			// col 0-31
		OLED_Show_Num(6,32,last_lng_d,3);		// col 32-55
		OLED_Show_Chinese(6,56,6);  // ° col 56-71
		OLED_Show_Num(6,72,last_lng_m,2);		// col 72-87
		OLED_Show_Char(6,88,'\'');				// col 88-95
		OLED_Show_Num(6,96,last_lng_s,2);		// col 96-111
		OLED_Show_Char(6,112,'"');				// col 112-119
		OLED_Show_Char(6,120,'E');				// col 120-127

        xSemaphoreGive(oledMutex);
        /* ===== OLED 显示结束 ===== */

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void GPS_Send_Task(void *arg)
{
    safe_printf("gps task is start\r\n");
	BaseType_t ret;
	GPS_Data_t gps_data;
	extern uint8_t uart2_rx_byte;
	__HAL_UART_CLEAR_OREFLAG(&huart2);
	HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
	while(1)
	{
		ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(500));
		ret = read_data_from_loop_buffer(&bd_uart_loop_buffer,&read_data);
		if(ret == 0)
		{
			safe_printf("len:%d,data:%s\r\n", read_data.len, read_data.data);
			if(strncmp((const char*)read_data.data,"$GNRMC",6) == 0)
			{
				parse_simple(&read_data,&gps_data);
				xQueueSend(gpsQueueHandle,&gps_data,0);
				char b3[128];
				safe_printf("[GPS]parsed lat=%d°%d\'%d\"N lng=%d°%d\'%d\"E bj=%d:%02d:%02d\r\n",
					gps_data.latitude_degree, gps_data.latitude_minute, gps_data.latitude_second,
					gps_data.longitude_degree, gps_data.longitude_minute, gps_data.longitude_second,
					gps_data.beijing_hour, gps_data.utc_minute, gps_data.utc_second);
				
			}
			else if(strncmp((const char*)read_data.data,"$GNGGA",6) == 0)
			{
				safe_printf("non-GNRMC len=%d,data=%s\r\n", read_data.len, read_data.data);
			}
		}		
	}
}

void GPSProject_Create(void)
{
    OLED_Init();
    OLED_FullOrClear(0);
	loop_buffer_init(&bd_uart_loop_buffer);

	dht11Handle_t = xQueueCreate(10,sizeof(DHT11_Data_t));
	if(dht11Handle_t == NULL)
	{
		printf("dht11 queue create failed\r\n");
	}

	gpsQueueHandle = xQueueCreate(10,sizeof(GPS_Data_t));
	if(gpsQueueHandle == NULL)
	{
		printf("gps queue create failed\r\n");
	}

	dht11ReadySem = xSemaphoreCreateBinary();
	if(dht11ReadySem == NULL)
	{
		printf("dht11 semaphore create failed\r\n");
	}

	printfMutex = xSemaphoreCreateMutex();
	if(printfMutex == NULL)
	{
		printf("printf mutex create failed\r\n");
	}

	oledMutex = xSemaphoreCreateMutex();
	if(oledMutex == NULL)
	{
		printf("oled mutex create failed\r\n");
	}

	xTaskCreate(DHT11_Send_Task,"DHT11 Send Task",256,NULL,2,NULL);
	xTaskCreate(OLED_Recv_Task,"OLED Recv Task",512,NULL,3,NULL);
	xTaskCreate(GPS_Send_Task,"GPS Send Task",512,NULL,3,&gpstaskHandle);
}
