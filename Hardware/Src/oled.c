#include "oled.h"

/**
 * @brief 向OLED发送命令
 * @param cmd 要发送的命令字节
 */
void OLED_Write_CMD(uint8_t cmd)
{
	//函数的参数解释：I2C的触发外设、从机地址、（从机内部子地址，这是命令or数据）、子地址长度（MemAddress 是 1 字节（8 位））、要发送的数据、字节数、超时
	HAL_I2C_Mem_Write(&hi2c2,0x78,0x00,I2C_MEMADD_SIZE_8BIT,&cmd,1,1000);
}

/**
 * @brief 向OLED发送数据
 * @param dat 要发送的数据字节
 */
void OLED_Write_Data(uint8_t dat)
{
	HAL_I2C_Mem_Write(&hi2c2,0x78,0x40,I2C_MEMADD_SIZE_8BIT,&dat,1,1000);
}

/**
 * @brief OLED初始化函数
 * @note 该函数会发送一系列初始化命令给OLED屏幕，以设置其工作模式和显示参数。具体的命令序列可以参考OLED的手册。
 */
void OLED_Init(void)
{
	uint8_t init_cmd[]={
	0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,0xA1, 0xC8,0xDA,
	0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,0x8D,0x14,
	0xAF};//规定的初始化命令，查手册得到
	
	for(int i = 0; i < 23; i++)
	{
		OLED_Write_CMD(init_cmd[i]);//执行初始化命令
	}
}

/**
 * @brief 填满或清空OLED屏幕
 * @param value 如果为非零值，则填满屏幕（写入0xFF），如果为零，则清空屏幕（写入0x00）。
 * @note 该函数会遍历OLED的每一页和每一列，并根据传入的参数决定是填满还是清空屏幕。
 */
void OLED_FullOrClear(uint8_t value)		//既可以填满：写0XFF  又可以清空：写0X00
{
	uint8_t dat = value? 0xFF:0x00;
	for(int j = 0;j < 8;j++)
	{
		OLED_Write_CMD(0xB0 + j);//OLED的第一页
		OLED_Write_CMD(0x00);//列的低四位
		OLED_Write_CMD(0x10);//列的高三位
		
		for(int i = 0;i < 128;i++)
		{
			OLED_Write_Data(dat);
		}
	}
}

/**
 * @brief 显示单个字符在OLED屏幕上
 * @param page OLED的页地址（0-7）
 * @param col OLED的列地址（0-127）
 * @param ch 要显示的字符
 * @note 该函数会在指定的页和列位置显示一个字符。字符的字模数据存储在ascii_16x8数组中，每个字符占用16个字节（8列，16行）。
 *       由于OLED屏幕的每页只能显示8行，因此一个字符需要分两页显示。函数会先显示字符的上半部分（前8行），然后显示下半部分（后8行）。
 */
void OLED_Show_Char(uint8_t page,uint8_t col,uint8_t ch)
{	//参数为  页  列  需要显示的字符
	uint8_t ch_index = ch - 32;//将字符的ASNCII码值再减去空格的就是该字符在该字模数组中对应的下标索引值
	//数据是8列16行，所以分为2页
	OLED_Write_CMD(0xB0 + page);
	OLED_Write_CMD(0x00 + (col&0x0f));//发送低四位数据
	OLED_Write_CMD(0x10 + ((col>>4) & 0x07));//发送高三位数据
	
	for(int i = 0;i < 8;i++)
	{
		OLED_Write_Data(ascii_16x8[ch_index][i]);
	}
	
	
	OLED_Write_CMD(0xB0 + page + 1);//下一页了，所以+1
	OLED_Write_CMD(0x00 + (col&0x0f));//0000 1111 &col后只会形成0000 后面的四位若col有0则出现0，保留了低四位
	OLED_Write_CMD(0x10 + ((col>>4) & 0x07));
	
	for(int i = 8;i < 16;i++)
	{
		OLED_Write_Data(ascii_16x8[ch_index][i]);
	}
	
}

/**
 * @brief 在OLED屏幕上显示字符串
 * @param page OLED的页地址（0-7）
 * @param col OLED的列地址（0-127）
 * @param string 要显示的字符串
 * @note 该函数会在指定的页和列位置显示一个字符串。字符串中的每个字符都会被单独显示。
 */
void OLED_Show_String(uint8_t page,uint8_t col,char* string)
{
	uint8_t col_offset = 0;
	while(*string != '\0')
	{
		if(col + col_offset > 120) break;//防止显示的字符超出 OLED 屏幕的列边界，避免越界显示导致的屏幕乱码或程序异常
		
		OLED_Show_Char(page,col+col_offset,*string);
		string++;
		col_offset += 8;
	}
}

/**
 * @brief 在OLED屏幕上显示数字
 * @param page OLED的页地址（0-7）
 * @param col OLED的列地址（0-127）
 * @param num 要显示的数字
 * @param len 要显示的数字长度
 * @note 该函数会将一个整数拆分为单个数字，并在指定的页和列位置显示这些数字。数字的显示顺序是从高位到低位。
 */
void OLED_Show_Num(uint8_t page,uint8_t col,uint32_t num,uint8_t len)
{
	uint8_t num_buf[8] = {0};
	uint8_t col_offset = 0;
	
	//拆分成一位数放到数组中（没有其他办法，数学方法没办法获取最高位）  
	//	顺序是低位->高位
	for(uint8_t i = 0;i<len;i++)
	{
		num_buf[i] = num % 10;
		num = num / 10;
	}
	
	//反向显示  顺序是从高位->低位
	for(uint8_t i = len;i>0;i--)
	{
		if((col + col_offset) > 127)	break;//防止想要显示的字符超出屏幕右侧
		OLED_Show_Char(page,col+col_offset,num_buf[i-1]+48);
		col_offset += 8;//毕竟一共才128列
	}	
}

/**
 * @brief 在OLED屏幕上显示汉字
 * @param page OLED的页地址（0-7）
 * @param col OLED的列地址（0-127）
 * @param idx 要显示的汉字在zh16x16数组中的索引
 * @note 该函数会在指定的页和列位置显示一个汉字。汉字的字模数据存储在zh16x16数组中，每个汉字占用32个字节（16列，16行）。
 *       由于OLED屏幕的每页只能显示8行，因此一个汉字需要分两页显示。函数会先显示汉字的上半部分（前8行），然后显示下半部分（后8行）。
 */
void OLED_Show_Chinese(uint8_t page,uint8_t col,uint8_t idx)
{	
	//就是显示字符函数的基础上需要修改，把每页显示8列改为显示16列（注意看取模就知道了）

	const unsigned char* cn_data = zh16x16[idx];
	
	//数据是8列16行，所以分为2页
	OLED_Write_CMD(0xB0 + page);
	OLED_Write_CMD(0x00 + (col&0x0f));//发送低四位数据
	OLED_Write_CMD(0x10 + ((col>>4) & 0x07));//发送高三位数据
	
	for(int i = 0;i < 16;i++)
	{
		OLED_Write_Data(cn_data[i]);
	}
		
	OLED_Write_CMD(0xB0 + page + 1);//下一页了，所以+1
	OLED_Write_CMD(0x00 + (col&0x0f));//0000 1111 &col后只会形成0000 后面的四位若col有0则出现0，保留了低四位
	OLED_Write_CMD(0x10 + ((col>>4) & 0x07));
	
	for(int i = 16;i < 32;i++)
	{
		OLED_Write_Data(cn_data[i]);
	}
	
}

/**
 * @brief 在OLED屏幕上显示浮点数
 * @param page OLED的页地址（0-7）
 * @param col OLED的列地址（0-127）
 * @param num 要显示的浮点数
 * @param int_len 整数部分的长度，如果为0，则自动适配整数长度
 * @param dec_len 小数部分的长度
 * @note 该函数会将一个浮点数拆分为整数部分和小数部分，并在指定的页和列位置显示这些数字。整数部分和小数部分的显示顺序是从高位到低位。
 */
void OLED_Show_Float(uint8_t page,uint8_t col,float num,uint8_t int_len,uint8_t dec_len)
{
	uint8_t num_buffer[8] = {0};	// 存储拆分后的数字
	uint8_t col_offset = 0;			// 列偏移量
	int32_t int_part = 0;			// 整数部分
	int32_t dec_part = 0;			// 小数部分
	float dec_temp = 0.0f;			// 临时存储小数部分
	
	// 拆分整数和小数部分
	int_part = (int32_t)num;	// 获取整数部分
	dec_temp = num - (float)int_part;// 获取小数部分
	uint8_t scale = 1;
	for(uint8_t i = 0;i<dec_len;i++)
	{
		scale *= 10;
	}
	dec_part = (int32_t)(dec_temp * scale + 0.5f);	// 四舍五入处理小数部分

	// 处理小数进位：例如 24.96 保留1位小数时 dec_part=10，应进为 25.0
	if(dec_part >= scale)
	{
		int_part += 1;
		dec_part -= scale;
	}
	
	// 显示整数部分
	if(int_len == 0)	//如果传入参数为0，那么自动适配整数长度
	{
		//计算整数部分的长度
		uint8_t temp_len = 0;
		int32_t temp_num = int_part;
		do
		{
			temp_len++;
			temp_num /= 10;
		}while(temp_num > 0 && temp_len <8);//最多显示8位整数，防止溢出

		//拆分整数部分的每一位数字（低位->高位）
		for(uint8_t i = 0;i<temp_len;i++)
		{
			num_buffer[i] = int_part % 10;
			int_part /= 10;
		}
		//反向显示整数部分（高位->低位）
		for(uint8_t i = temp_len;i>0;i--)
		{
			if((col + col_offset) > 127)	break;//防止想要显示的字符超出屏幕右侧
			OLED_Show_Char(page,col+col_offset,num_buffer[i-1]+48);//+48是因为ASCII码中数字0的值是48
			col_offset += 8;//毕竟一共才128列
		}
	}
	else	//如果传入参数不为0，那么按照传入的整数长度显示
	{	
		//与上面的同理
		for(uint8_t i = 0;i < int_len;i++)
		{
			num_buffer[i] = int_part % 10;
			int_part /= 10;
		}
		for(uint8_t i = int_len;i>0;i--)
		{
			if((col + col_offset) > 127)	break;//防止想要显示的字符超出屏幕右侧
			OLED_Show_Char(page,col+col_offset,num_buffer[i-1]+48);//+48是因为ASCII码中数字0的值是48
			col_offset += 8;//毕竟一共才128列
		}
	}

	//显示小数点
	if(dec_len > 0 && (col + col_offset) <= 127)
	{
		OLED_Show_Char(page,col+col_offset,'.');
		col_offset += 8;
	}

	//显示小数部分
	if(dec_len > 0)
	{
		//拆分小数部分的每一位数字（低位->高位）
		for(uint8_t i = 0;i < dec_len;i++)
		{
			num_buffer[i] = dec_part % 10;
			dec_part /= 10;
		}
		//反向显示小数部分（高位->低位）
		for(uint8_t i = dec_len;i>0;i--)
		{
		if((col + col_offset) > 127)	break;//防止想要显示的字符超出屏幕右侧
			OLED_Show_Char(page,col+col_offset,num_buffer[i-1]+48);
			col_offset += 8;//毕竟一共才128列
		}
	}

}

/**
 * @brief 在OLED屏幕上显示位图
 * @param x0 起始列地址（0-127）
 * @param y0 起始行地址（0-63）
 * @param x1 结束列地址（0-127）
 * @param y1 结束行地址（0-63）
 * @param BMP 位图数据数组
 * @note 该函数会在指定的区域内显示一个位图。位图数据存储在BMP数组中，每个字节表示8行像素。函数会根据传入的起始和结束坐标计算需要显示的页数和列数，并将位图数据写入OLED屏幕。
 */
void OLED_Show_BMP(uint8_t x0,uint8_t y0, uint8_t x1,uint8_t y1,uint8_t BMP[])
{
	uint8_t width = x1-x0+1;   //宽度

	uint8_t  start_page = y0 / 8;  //起始页   
	uint8_t stop_page = y1 / 8;  //结束页

	uint8_t page_count = stop_page - start_page + 1;
	uint8_t x, y; 

	for(y = 0; y < page_count;y++)
	{
		//设置OLED显示位置
		OLED_Write_CMD(0xB0+y+start_page);//页地址
		OLED_Write_CMD(0x00 + (x0 & 0x0f));//列的低四位
		OLED_Write_CMD(0x10 + ((x0>>4)&0x07));//列的高三位

		// 计算该页数据在位图数组中的起始索引
		// 如果是列主序：每一页有 width 个字节
		uint32_t page_offset = y * width;

		for(x = 0; x < width;x++)
		{
			//计算该页的数据在位图中的起始索引 
			OLED_Write_Data(BMP[page_offset + x]); 
		}
	}
  

}

void OLED_Test(void)
{
	OLED_Init();
	
//	OLED_Write_CMD(0xB0);//OLED的第一页
//	OLED_Write_CMD(0x00);//列的低四位
//	OLED_Write_CMD(0x10);//列的高三位
//	
//	for(int i = 0;i < 127;i++)
//	{
//		OLED_Write_Data(0xAA);
//	}
	
	
	HAL_Delay(1000);
	
	
}
