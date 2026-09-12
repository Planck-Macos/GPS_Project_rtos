#include "dht11.h"


/**
 * @brief  DHT11引脚配置为输出模式
 * @param  None
 */
void DHT11_SetOutput(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}


/**
 * @brief  DHT11引脚配置为输入模式
 * @param  None
 */
void DHT11_SetInput(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/**
 * @brief  DHT11起始信号
 * @param  None
 */
void DHT11_Start(void)
{
  DHT11_SetOutput();
  
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET);//拉低数据线的电平，持续至少18ms
  
  delay_ms(20);

}

/**
 * @brief  DHT11等待响应信号 * @param  None
 * @return uint8_t 0:成功，1:失败
 */
uint8_t DHT11_Check_Ack(void)
{
  uint8_t time = 0;//加上超时判断，防止死循环
  DHT11_SetInput();
  
  while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9) == GPIO_PIN_SET && time < 100)
  {//等待DHT11拉低数据线的电平，所以一直检测到不是高电平了再跳出循环
    
    time++;
    delay_us(1);
  }
  
  if(time >= 100)//超时判断
  {
    return 1;
  }
  
  time = 0;
  
  while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9) == GPIO_PIN_RESET && time < 100)
  {//等待DHT11拉高数据线的电平，所以一直检测到不是低电平了再跳出循环
  
    time++;
    delay_us(1);
  }
  if(time >= 100)//超时判断
  {
    return 1;
  }
  time = 0;
  
  //根据数据手册可以得知，DHT11在发送完响应信号后会再次拉低数据线的电平，
  //之后才会拉高数据线的电平输出数据，所以这里再次等待DHT11拉低数据线的电平
  while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9) == GPIO_PIN_SET && time < 100)
  {//等待DHT11拉低数据线的电平，所以一直检测到不是高电平了再跳出循环
    
    time++;
    delay_us(1);
  }
  
  if(time >= 100)//超时判断
  {
    return 1;
  }
  
  time = 0;
  
  
  return 0;

}



uint8_t DHT11_Read_Bit(void)
{
  uint8_t time = 0;
  uint8_t level;
  //等待低电平结束
  while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9) == GPIO_PIN_RESET && time < 100)
  {
  
    time++;
    delay_us(1);
  }
  delay_us(35);//等待35us后读取数据线的电平，如果是高电平则表示读取到的是1，如果是低电平则表示读取到的是0
  
  if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9) == GPIO_PIN_SET)
  {
      level = 1;
  
  }
  else
  {
     level = 0;
  }
  
  //等待高电平结束
  time = 0;
  while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9) == GPIO_PIN_SET && time < 100)
  {
    
    time++;
    delay_us(1);
  }
  

  
  return level;
  
  
}

//1011  1111

uint8_t DHT11_Read_Byte(void)
{
  uint8_t Byte_data = 0;
  uint8_t i;
  
  for(i = 0;i<8;i++) // 1 ->  10 -> 101 ->1011    
  {
    Byte_data =Byte_data << 1;			//	0 -> 10 -> 100 -> 1010
    Byte_data |= DHT11_Read_Bit();		//	1 -> 10 -> 101 -> 1011
  }
  
  return Byte_data;
}





//void DHT11_Read(void)
//{
//  uint8_t humi_h,humi_l,temp_h,temp_l,crc,make_crc;
//  float humidity,temperature;
//  DHT11_Start();
//  if(DHT11_Check_Ack() == 0)
//  {
//    humi_h = DHT11_Read_Byte();//读取湿度整数部分
//    humi_l = DHT11_Read_Byte();//读取湿度小数部分
//    temp_h = DHT11_Read_Byte();//读取温度整数部分
//    temp_l = DHT11_Read_Byte();//读取温度小数部分
//    crc = DHT11_Read_Byte();//读取校验值
//    
//  }
//  
//  make_crc = humi_h + humi_l + temp_h + temp_l;
//  
//  if(make_crc == crc)//校验数据正确
//  {
//    humidity = humi_h + (humi_l/10.0);
//    temperature = temp_h + (temp_l/10.0);
//    
//    printf("湿度：%.1f %%RH,温度：%.1f ℃\r\n",humidity,temperature);
//  }
//  else
//  {
//    printf("校验位有错误\r\n");
//  }
//  
//}


//改为地址传递来进行修改内容，其余保持不变
int DHT11_Read_Data(float *humidity,float *temperature)
{
  uint8_t humi_h,humi_l,temp_h,temp_l,crc,make_crc;
  DHT11_Start();
  if(DHT11_Check_Ack() == 0)
  {
    humi_h = DHT11_Read_Byte();//读取湿度整数部分
    humi_l = DHT11_Read_Byte();//读取湿度小数部分
    temp_h = DHT11_Read_Byte();//读取温度整数部分
    temp_l = DHT11_Read_Byte();//读取温度小数部分
    crc = DHT11_Read_Byte();//读取校验值
    
  }
  
  make_crc = humi_h + humi_l + temp_h + temp_l;
  
  if(make_crc == crc)//校验数据正确
  {
    *humidity = humi_h + (humi_l/10.0);
    *temperature = temp_h + (temp_l/10.0);
    
    return 0;//表示成功
  }
  return -1;//表示失败
}


