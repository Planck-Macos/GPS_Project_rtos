#ifndef __OLED_H__
#define __OLED_H__

#include "stm32f1xx_hal.h"
#include "i2c.h"
#include "font.h"

void OLED_Init(void);
void OLED_FullOrClear(uint8_t value);
void OLED_Show_Char(uint8_t page,uint8_t col,uint8_t ch);
void OLED_Show_String(uint8_t page,uint8_t col,char* string);
void OLED_Show_Num(uint8_t page,uint8_t col,uint32_t num,uint8_t len);
void OLED_Show_Chinese(uint8_t page,uint8_t col,uint8_t idx);
void OLED_Show_Float(uint8_t page,uint8_t col,float num,uint8_t int_len,uint8_t dec_len);
void OLED_Show_BMP(uint8_t x0,uint8_t y0, uint8_t x1,uint8_t y1,uint8_t BMP[]);
void OLED_Test(void);

#endif


