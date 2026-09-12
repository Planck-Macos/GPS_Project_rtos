#include "loop_buffer.h"

/**
 * @brief 初始化循环缓冲区
 * @param buffer 循环缓冲区指针
 * @note 该函数会初始化循环缓冲区，将读取和写入索引设置为0，并将所有数据节点的长度设置为0。
 */
void loop_buffer_init(loop_buffer_t *buffer)
{
	buffer->read_index=0;
	buffer->write_index=0;
	
	for(int i=0;i<MAX_NODE_NUM;i++)
	{
		buffer->note[i].len=0;
		for(int j=0;j<MAX_SIZE;j++)
		{
			buffer->note[i].data[j]=0;
		}
	}
}

/**
 * @brief 将数据写⼊循环缓冲区
 * @param buffer 循环缓冲区指针
 * @param dat 要写⼊的note_t数据指针
 * @return 成功返回0，缓冲区满返回-1
 * @note 该函数会将一个note_t类型的数据写入循环缓冲区。如果缓冲区已满，则返回-1表示写入失败；否则，将数据复制到缓冲区的当前写入位置，并更新写入索引。
 */
int write_data_to_loop_buffer(loop_buffer_t *buffer, note_t *dat)
{
	uint8_t next_index = (buffer->write_index+1) % MAX_NODE_NUM;
	
	if(next_index == buffer->read_index)//判满
	{
		return -1;
	}
	
	memcpy(&buffer->note[buffer->write_index],dat,sizeof(note_t));//复制数据到缓冲区
	
	buffer->write_index = next_index;//更新索引值
	
	return 0;
}

/**
 * @brief 从循环缓冲区读取数据
 * @param buffer 循环缓冲区指针
 * @param dat 用于存储读取数据的note_t指针
 * @return 成功返回0，缓冲区空返回-1
 * @note 该函数会从循环缓冲区读取一个note_t类型的数据。如果缓冲区为空，则返回-1表示读取失败；否则，将数据复制到指定位置，并更新读取索引。
 */
int read_data_from_loop_buffer(loop_buffer_t *buffer, note_t *dat)
{
	if(buffer->write_index==buffer->read_index)//判空
	{
		return -1;
	}
	
	memcpy(dat,&buffer->note[buffer->read_index],sizeof(note_t));//从缓冲区当前位置复制数据
	
	buffer->read_index = (buffer->read_index+1)%MAX_NODE_NUM;//更新索引值

	return 0;
}




