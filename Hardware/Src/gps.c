#include "gps.h"


void parse_simple(note_t *note,GPS_Data_t *gps_data)
{
    if(note == NULL || note->len == 0)  // 第一步：校验参数有效性
    {
        printf("参数错误：note为空或数据长度为0\r\n");
        return;
    }

    char *data = (char *)note->data;
    // $GNRMC,161414.400,A,3906.71994,N,11703.86380,E,000.0,000.0,181225,,,A*71 
    char fields[13][20] = {0};// 第一维存储每个字段，第二维存储每个字段的字符位数
    int fields_count = 0;// 记录字段数量
    int char_count = 0;// 记录当前字段的字符数

    // 先完整分割所有字段（仅分割，不解析）
    for(int i = 0; data[i] != '\0' && fields_count < 12; i++ )
    {
        if(data[i] == ',')
        {
            fields[fields_count][char_count] = '\0'; // 字段结束，加终止符
            fields_count++;
            char_count = 0;
        }
        else
        {
            if(char_count <= 19) // 限制单字段长度，避免越界
            {
                fields[fields_count][char_count] = data[i];
                char_count++;
            }
        }
    }
    // 处理最后一个字段（无末尾逗号的情况）
    if (char_count > 0 && fields_count < 13)
    {
        fields[fields_count][char_count] = '\0';
        fields_count++;
    }


    // 解析时间
     gps_data -> utc_hour = (fields[1][0] - '0')*10 +  (fields[1][1] - '0'); //小时
     gps_data -> utc_minute =  (fields[1][2] - '0')*10 +  (fields[1][3] - '0'); //分钟
     gps_data -> utc_second =  (fields[1][4] - '0')*10 +  (fields[1][5] - '0'); //秒
     gps_data -> beijing_hour = (gps_data -> utc_hour + 8) % 24; //北京时间 = UTC时间 + 8小时


    // 解析纬度
    gps_data -> latitude = 0.0f;
    char *point_pos = strchr(fields[3], '.');
    // 校验纬度字段有效性
    if(point_pos == NULL || strlen(fields[3]) < 4) // 至少ddmm格式（3906）
    {
        printf("纬度字段格式错误：%s\r\n", fields[3]);
        return;
    }
    // 正确解析ddmm.sssss格式
    gps_data -> latitude_degree = (fields[3][0] - '0')*10 + (fields[3][1] - '0'); // 度（39）
    double lat_minute_part = atof(fields[3] + 2); // 分+小数（06.71994）atof函数将字符串转换为浮点数
    gps_data -> latitude = gps_data -> latitude_degree + lat_minute_part / 60.0; // 转十进制度

    // 度分秒格式转换
    gps_data -> latitude_minute = (int)lat_minute_part; // 分（6）
    gps_data -> latitude_second = (lat_minute_part - gps_data -> latitude_minute) * 60; // 秒（0.71994*60≈43.196）


    //解析经度
	gps_data -> longitude = 0.0f;
	char *point_pos2 = strchr(fields[5],'.');
    //校验经度字段有效性
	if(point_pos2 == NULL || strlen(fields[5]) < 4)
	{
		printf("经度字段格式错误：%s\r\n",fields[5]);
		return;
	}
    // 正确解析dddmm.sssss格式
	gps_data ->longitude_degree = (fields[5][0]- '0') * 10 + (fields[5][1]- '0');
	double longitude_minute_part = atof(fields[5] + 2);
	gps_data ->longitude = gps_data ->longitude_degree + longitude_minute_part / 60.0f;

	// 度分秒格式转换
	gps_data ->longitude_minute = (int)longitude_minute_part;
	gps_data ->longitude_second = (longitude_minute_part - gps_data->longitude_minute)*60;
	
}

