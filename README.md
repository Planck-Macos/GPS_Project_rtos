# GPS_Project_rtos

基于 **STM32F103RCT6 + FreeRTOS** 的北斗定位显示系统。通过 FreeRTOS 多任务调度，实现 DHT11 温湿度采集、GPS NMEA 语句解析、OLED 多信息显示，并使用队列、信号量、互斥量完成任务间同步与通信。

## 功能特性

- ?? **温湿度采集**：DHT11 单总线协议，2 秒采集周期
- ?? **北斗/GPS 定位**：USART2 接收 NMEA 语句，解析 `$GNRMC` 获取经纬度与北京时间
- ? **OLED 显示**：SSD1306（I2C），显示温湿度 + 经纬度（度分秒格式）
- ? **RTOS 调度**：FreeRTOS 抢占式调度，3 个业务任务并行运行
- ? **线程安全**：互斥量保护 printf 与 OLED 共享资源

## 硬件平台

| 器件 | 型号 | 接口 |
|------|------|------|
| MCU | STM32F103RCT6 (Cortex-M3, 72MHz) | - |
| 温湿度传感器 | DHT11 | GPIO (PB9) |
| GPS/北斗模块 | NMEA-0183, 9600bps | USART2 (PA2/PA3) |
| OLED 显示屏 | SSD1306 0.96寸, I2C | I2C2 (PB10/PB11) |
| 调试串口 | - | USART1 (PA9/PA10), 115200 |
| 系统时基 | TIM4 | 1ms tick (HAL) |
| 微秒延时 | TIM5 | PSC=71, 1?s/计数 |

## 软件架构

```
┌─────────────────────────────────────────────────┐
│                   应用层 (FSP)                    │
│  DHT11_Send_Task  OLED_Recv_Task  GPS_Send_Task  │
└──────┬───────────────┬───────────────┬──────────┘
       │ 队列           │ 队列           │ 任务通知
       ▼               ▼               ▼
┌─────────────────────────────────────────────────┐
│               板级驱动层 (Hardware)               │
│  dht11.c   oled.c   gps.c   loop_buffer.c delay.c│
└──────┬───────────────┬───────────────┬──────────┘
       │               │               │
       ▼               ▼               ▼
┌─────────────────────────────────────────────────┐
│            HAL + FreeRTOS + CMSIS                 │
│    Core / Drivers / Middlewares/Third_Party       │
└─────────────────────────────────────────────────┘
```

### 任务间通信

| 通信链路 | 同步原语 | 数据类型 |
|---------|---------|---------|
| DHT11 → OLED | 消息队列 + 二值信号量 | `DHT11_Data_t` |
| GPS 中断 → GPS 任务 | 任务通知 (`xTaskNotifyFromISR`) | 仅通知 |
| GPS → OLED | 消息队列 | `GPS_Data_t` |
| 多任务 → USART1 | 互斥量 (`safe_printf`) | - |
| OLED 任务 → I2C2 | 互斥量 (`oledMutex`) | - |

### 任务列表

| 任务 | 优先级 | 栈大小 | 职责 |
|------|-------|-------|------|
| DHT11_Send_Task | 2 | 256 words | 周期采集温湿度 → 入队 → 信号量通知 |
| OLED_Recv_Task | 3 | 512 words | 接收温湿度/经纬度 → 刷新 OLED |
| GPS_Send_Task | 3 | 512 words | 等待通知 → 读循环缓冲 → 解析 GNRMC → 入队 |

## 目录结构

```
project/
├── Core/                 # CubeMX 生成代码（HAL 初始化、中断、FreeRTOS 配置）
├── Drivers/              # STM32F1 HAL + CMSIS
├── FSP/                  # 应用任务层（任务、队列、同步原语）
│   ├── Inc/queue_task.h
│   └── Src/queue_task.c
├── Hardware/             # 板级驱动（BSP）
│   ├── Inc/  delay.h  dht11.h  font.h  gps.h  loop_buffer.h  oled.h
│   └── Src/  delay.c  dht11.c  font.c  gps.c  loop_buffer.c  oled.c
├── MDK-ARM/              # Keil 工程
├── Middlewares/          # FreeRTOS 源码
└── project.ioc           # CubeMX 配置文件
```

## 编译与烧录

1. 用 **Keil MDK-ARM 5** 打开 `MDK-ARM/project.uvprojx`
2. 编译生成 `MDK-ARM/project.axf`
3. 通过 ST-Link 下载到 STM32F103RCT6
4. 串口工具连接 **USART1 (115200)** 查看调试日志

## 显示效果

```
湿度：52.5%RH
温度：25.3度
lat=22°33'12"N
lng=113°56'78"E
```

> 室内无卫星信号时经纬度显示为 `0°0'0"`，拿到室外开阔地带即可定位。

## NMEA 解析说明

解析 `$GNRMC` 语句，提取：
- **UTC 时间** → 北京时间（UTC+8）
- **纬度** `ddmm.mmmmm` → 度°分′秒″
- **经度** `dddmm.mmmmm` → 度°分′秒″

## 许可证

MIT License
