#include "delay.h"


//计算公式：最大定时时长=(PSC+1)*(ARR+1)/时钟主频
void delay_us(uint32_t us) //PSC=72-1(保证每次计数都是1us),ARR=65535(尽量保证单次延时的最大时长)
{
  HAL_TIM_Base_Stop(&htim5);              // 先停止，确保State回到READY
  __HAL_TIM_SET_COUNTER(&htim5,0);        // 清空计数器
  HAL_TIM_Base_Start(&htim5);             // 启动定时器
  uint32_t timeout = 0;
  while(__HAL_TIM_GET_COUNTER(&htim5) < us)
  {
    if(++timeout > 1000000) break;        // 软超时保护，防止TIM5异常时死锁
  }
  HAL_TIM_Base_Stop(&htim5);              // 停止计数
}



void delay_ms(uint32_t ms)
{
  while(ms > 0)
  {
    if(ms >= 65)
    {
      delay_us(65535);
      ms -= 65;
    }
    else
    {
      delay_us(1000 * ms);
      ms = 0;
    }
    
    
  }

}
