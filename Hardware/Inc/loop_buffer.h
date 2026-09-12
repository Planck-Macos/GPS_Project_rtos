#ifndef __LOOP_BUFFER_H__
#define __LOOP_BUFFER_H__

#include "stm32f1xx_hal.h"
#include <string.h>

#define MAX_SIZE (128)			// 定义每个数据节点的最大长度
typedef struct {
	uint8_t data[MAX_SIZE];// 数据数组
	uint16_t len;		   // 数据长度
} note_t;			// 定义数据节点结构体，包含数据数组和长度字段


#define MAX_NODE_NUM (10)		// 定义循环缓冲区的最大节点数
typedef struct {
	note_t note[MAX_NODE_NUM];	// 数据节点数组
	uint8_t write_index;		// 写入索引
	uint8_t read_index;			// 读取索引
} loop_buffer_t;		// 定义循环缓冲区结构体，包含数据节点数组、写入索引和读取索引


// 函数声明
void loop_buffer_init(loop_buffer_t *buffer);
int write_data_to_loop_buffer(loop_buffer_t *buffer, note_t *dat);
int read_data_from_loop_buffer(loop_buffer_t *buffer, note_t *dat);

#endif /* __LOOP_BUFFER_H_*/

