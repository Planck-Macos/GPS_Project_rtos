# GPS_Project_rtos

基于 **STM32F103RCT6 + FreeRTOS** 的北斗定位显示系统。通过 FreeRTOS 多任务调度，实现 DHT11 温湿度采集、GPS NMEA 语句解析、OLED 多信息显示，并使用队列、信号量、互斥量、任务通知完成任务间同步与通信。

> 本项目是一个 STM32 + RTOS 的入门实战项目，重点在于**多任务架构设计**与**任务间通信**，README 中还整理了完整的踩坑 FAQ，供遇到同类问题的朋友参考。

## 目录

- [功能特性](#功能特性)
- [硬件平台](#硬件平台)
- [工程架构](#工程架构)
- [任务与通信机制](#任务与通信机制)
- [目录结构](#目录结构)
- [关键设计说明](#关键设计说明)
- [编译与烧录](#编译与烧录)
- [FAQ：踩坑记录](#faq踩坑记录)
- [NMEA 解析说明](#nmea-解析说明)
- [许可证](#许可证)

## 功能特性

- **温湿度采集**：DHT11 单总线协议，2 秒采集周期
- **北斗/GPS 定位**：USART2 中断接收 NMEA 语句，解析 `$GNRMC` 获取经纬度与北京时间（UTC+8）
- **OLED 显示**：SSD1306（I2C），同屏显示温湿度 + 经纬度（度分秒格式）
- **RTOS 调度**：FreeRTOS 抢占式调度，3 个业务任务并行运行
- **线程安全**：互斥量保护 printf（`safe_printf`）与 OLED I2C 总线（`oledMutex`）

## 硬件平台

| 器件 | 型号/说明 | 接口 |
|------|------|------|
| MCU | STM32F103RCT6 (Cortex-M3, 72MHz, 256KB Flash) | - |
| 温湿度传感器 | DHT11 单总线 | GPIO PB9 |
| GPS/北斗模块 | NMEA-0183, **9600bps** | USART2 (PA2-TX / PA3-RX) |
| OLED 显示屏 | SSD1306 0.96 寸, I2C 地址 0x78 | I2C2 (PB10-SCL / PB11-SDA) |
| 调试串口 | USB-TTL | USART1 (PA9-TX / PA10-RX), 115200 |
| 系统时基 | TIM4 | 1ms tick (HAL) |
| 微秒延时 | TIM5 | PSC=71 → 1 计数 = 1?s（DHT11 时序） |

**接线要点**：USB-TTL 与 MCU 的 TX/RX 必须**交叉连接**（TTL-TX → PA10，TTL-RX → PA9），且必须**共地**。

## 工程架构

### 架构分层总览

工程自上而下分为 4 层，上层只调用下层 API，依赖方向单向向下：

```
┌─────────────────────────────────────────────────────────────┐
│  应用层  FSP/Src/queue_task.c                                │
│  业务任务 + 任务间通信（队列/信号量/互斥量/任务通知）             │
│  GPSProject_Create()  DHT11_Send_Task  OLED_Recv_Task        │
│  GPS_Send_Task  safe_printf()                                │
├─────────────────────────────────────────────────────────────┤
│  板级驱动层 (BSP)  Hardware/                                 │
│  面向具体器件的驱动，不感知 RTOS 任务                          │
│  dht11.c    DHT11 单总线时序（依赖 delay_us）                 │
│  gps.c      NMEA-0183 语句解析（$GNRMC 字段分割）             │
│  oled.c     SSD1306 I2C 驱动 + 字符/数字/浮点/汉字显示         │
│  loop_buffer.c  串口环形缓冲区（中断 ? 任务 共享）             │
│  delay.c    TIM5 微秒延时   font.c  ASCII/汉字字库            │
├─────────────────────────────────────────────────────────────┤
│  RTOS 内核层  Middlewares/Third_Party/FreeRTOS               │
│  任务调度 / 队列 / 信号量 / 互斥量 / 任务通知 / 堆管理(heap_4)  │
├─────────────────────────────────────────────────────────────┤
│  HAL 硬件抽象层  Core/ + Drivers/                            │
│  Core:     CubeMX 生成（main、外设初始化、中断服务、时钟配置）   │
│  Drivers:  STM32F1xx HAL 库 + CMSIS                          │
└─────────────────────────────────────────────────────────────┘
```

### 各层职责

| 层 | 位置 | 职责 | 典型 API |
|----|------|------|---------|
| 应用层 | `FSP/` | 创建任务、建立任务间通信、业务逻辑编排 | `GPSProject_Create()` |
| 驱动层 | `Hardware/` | 操作具体器件，提供原子化的读/写/解析函数 | `DHT11_Read_Data()`、`parse_simple()`、`OLED_Show_Float()` |
| 内核层 | `Middlewares/` | 调度与 IPC 原语 | `xQueueSend()`、`xSemaphoreTake()`、`xTaskNotifyFromISR()` |
| HAL 层 | `Core/` + `Drivers/` | 外设寄存器初始化、中断入口、时钟 | `HAL_I2C_Mem_Write()`、`HAL_UART_Receive_IT()` |

### 系统数据流

```
                 ┌─────────────────── USART2 中断（逐字节）───────────────────┐
                 │  HAL_UART_RxCpltCallback                                  │
                 │  字节写入 loop_buffer → 检测到帧尾'\n' →                    │
                 │  xTaskNotifyFromISR(gpstaskHandle) + portYIELD_FROM_ISR   │
                 ▼                                                           │
DHT11_Send_Task                                          GPS_Send_Task       │
 (优先级2, 512w)                                          (优先级3, 512w)     │
 DHT11_Read_Data                                          ulTaskNotifyTake   │
      │                                                   读循环缓冲区一帧     │
      ▼                                                   strncmp "$GNRMC"   │
dht11Handle_t(队列)                                        parse_simple 解析  │
      │                                                          │           │
      │ dht11ReadySem(二值信号量,通知"新数据就绪")                   ▼           │
      │                                              gpsQueueHandle(队列)      │
      ▼                                                          ▼           ▼
                                              OLED_Recv_Task (优先级3, 512w)
                                               非阻塞收两个队列 + last_* 缓存
                                                          │
                                              oledMutex(互斥量保护I2C2)
                                                          ▼
                                                     SSD1306 刷新(500ms)

所有任务 ── printfMutex(互斥量) ──> safe_printf() ──> USART1 调试输出
```

### 初始化顺序（约定）

外设初始化 → 同步对象创建 → 任务创建 → 启动调度器，顺序不能颠倒：

```
main()
 ├─ HAL_Init / SystemClock_Config / GPIO / TIM4 / TIM5 / I2C2 / USART1 / USART2
 ├─ MX_FREERTOS_Init()
 │    └─ GPSProject_Create()
 │         ├─ OLED_Init() + OLED_FullOrClear(0)     ① 外设先就绪
 │         ├─ loop_buffer_init()                    ② 缓冲区先初始化
 │         ├─ xQueueCreate × 2                      ③ 队列：dht11Handle_t / gpsQueueHandle
 │         ├─ xSemaphoreCreate × 3                  ④ dht11ReadySem / printfMutex / oledMutex
 │         └─ xTaskCreate × 3                       ⑤ 最后创建任务
 └─ osKernelStart()                                 ⑥ 调度器启动，任务开始运行
```

## 任务与通信机制

| 任务 | 优先级 | 栈大小 | 职责 |
|------|-------|-------|------|
| DHT11_Send_Task | 2 | 512 words | 周期采集温湿度 → 入队 → 信号量通知 OLED 任务 |
| GPS_Send_Task | 3 | 512 words | 等待任务通知 → 读循环缓冲 → 解析 GNRMC → 入队 |
| OLED_Recv_Task | 3 | 512 words | 非阻塞收两个队列 → 缓存最近值 → 刷新 OLED |

| 通信链路 | 机制 | 为什么这样选 |
|---------|------|------------|
| GPS 中断 → GPS 任务 | 任务通知 `xTaskNotifyFromISR` + `portYIELD_FROM_ISR` | 只传"有数据了"的信号不传数据，任务通知最轻量 |
| GPS 任务 → OLED 任务 | 消息队列 `gpsQueueHandle`（深度10, `GPS_Data_t`） | 传结构体数据 |
| DHT11 → OLED | 队列 `dht11Handle_t` + 二值信号量 `dht11ReadySem` | 队列传数据，信号量解决"先发后收"的顺序同步 |
| 多任务 → USART1 | 互斥量 `printfMutex` | 共享资源（串口）必须串行化 |
| OLED 任务 → I2C2 | 互斥量 `oledMutex` | 共享资源（I2C 总线）必须串行化 |

## 目录结构

```
project/
├── Core/                 # CubeMX 生成代码（main、外设初始化、中断、FreeRTOS 配置）
├── Drivers/              # STM32F1 HAL 库 + CMSIS
├── FSP/                  # 应用任务层
│   ├── Inc/queue_task.h  #   DHT11_Data_t 定义、GPSProject_Create 声明
│   └── Src/queue_task.c  #   三个任务、safe_printf、GPSProject_Create
├── Hardware/             # 板级驱动层（BSP）
│   ├── Inc/              #   delay.h dht11.h font.h gps.h loop_buffer.h oled.h
│   └── Src/              #   delay.c dht11.c font.c gps.c loop_buffer.c oled.c
├── MDK-ARM/              # Keil MDK 工程（project.uvprojx）
├── Middlewares/          # FreeRTOS 内核源码（heap_4、ARM_CM3 port）
└── project.ioc           # CubeMX 配置文件
```

## 关键设计说明

### 线程安全的 printf（safe_printf）

```c
void safe_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (printfMutex != NULL) xSemaphoreTake(printfMutex, portMAX_DELAY);
    vprintf(fmt, args);
    if (printfMutex != NULL) xSemaphoreGive(printfMutex);
    va_end(args);
}
```

- 多个任务并发调用 `printf` 时，输出会交叉错乱；且 `printf` 与 `HAL_UART_Transmit` 共享 `huart->gState`，并发时会**静默丢输出**
- 用互斥量把"整条日志"变成原子操作，任务内一律用 `safe_printf` 代替 `printf`
- 用**变参函数**而不是变参宏：Keil ARMCC V5（C90）不支持 `__VA_ARGS__`
- 对 `printfMutex` 判空：调度器启动前（互斥量尚未创建时）调用也不会卡死

### GPS 接收链路：循环缓冲 + 任务通知

- 中断里只做三件事：写环形缓冲、判断帧尾、发任务通知，**绝不在中断里解析**
- GPS 任务 `ulTaskNotifyTake(pdTRUE, 500ms)` 醒来后从环形缓冲读出一整帧再解析
- 环形缓冲（`loop_buffer`）解耦"中断产生数据的速度"与"任务消费数据的速度"

### OLED 刷新策略：非阻塞 + 最近值缓存

- 两个队列都用**超时为 0 的非阻塞接收**，配合 `last_*` 缓存最近一次有效值
- 好处：任一数据源（DHT11/GPS）暂时无新数据时，屏幕保持上次内容而不是清空；单次循环最多 1s，不长时间阻塞
- 整段绘制用 `oledMutex` 保护，避免与其他任务抢占 I2C 总线导致花屏

## 编译与烧录

1. 用 **Keil MDK-ARM 5** 打开 `MDK-ARM/project.uvprojx`
2. 编译（F7），通过 ST-Link 下载到 STM32F103RCT6
3. 串口助手连接 **USART1 (115200, 8N1, UTF-8 编码)** 查看调试日志
4. `project.ioc` 可用 CubeMX 打开重新生成外设初始化代码

## 显示效果

```
湿度：52.5%RH
温度：25.3度
lat=22°33'12"N
lng=113°56'78"E
```

> 室内无卫星信号时经纬度显示 `0°0'0"`，属正常现象，拿到室外开阔地带即可定位（冷启动需几分钟）。

## FAQ：踩坑记录

开发过程中真实踩过的坑，按 **现象 → 原因 → 解决** 整理。

### 软件 / RTOS 类

**1. 多任务并发打印，串口输出交叉错乱、某几条日志整句消失**
- 现象：多个任务同时打印时，两行日志字符互相穿插；偶发某条日志完全没打出来
- 原因：① `printf` 不是线程安全的，多任务同时输出字符流互相穿插；② `printf` 与 `HAL_UART_Transmit` 共享 `huart->gState`，一个任务正在发送时，另一个任务的重定向 printf 会被静默丢弃
- 解决：封装 `safe_printf()`，用 `printfMutex` 互斥量把整条日志串行化；所有任务一律用 `safe_printf` 代替 `printf`

**2. 变参宏编译报错 `#969: the identifier __VA_ARGS__ can only appear...`**
- 现象：想用 `#define LOG(fmt, ...) printf(__VA_ARGS__)` 包装带互斥锁的打印，编译报错
- 原因：Keil ARMCC V5 编译器（C90 标准）不支持 `__VA_ARGS__` 变参宏
- 解决：不用宏，直接写变参函数 `safe_printf(const char *fmt, ...)`，内部用 `va_list + vprintf` 转发参数

**3. 任务只打印第一条日志就"卡死"**
- 现象：`dht11 task is start` 打印一次后再无任何输出，系统看似死机
- 原因：任务栈只有 256 words (1KB)，`printf` 打印 `%f` 浮点时栈开销很大，栈溢出破坏了内存
- 解决：涉及浮点打印的任务栈加大到 **512 words**；可用 `uxTaskGetStackHighWaterMark()` 检查栈余量

**4. GPS 串口有数据，但 GPS 任务永远收不到**
- 现象：示波器/逻辑分析仪确认 USART2 有波形，任务却从不醒来
- 原因：接收完成回调函数名拼写错误（写成了 `HAL_UART_RxCplCallback`），HAL 找不到 `HAL_UART_RxCpltCallback` 弱定义符号就不会调用
- 解决：回调名必须精确为 `HAL_UART_RxCpltCallback`（注意 `Cplt` 的 `t`）

**5. 队列/信号量/任务创建返回 NULL**
- 现象：`xQueueCreate` 返回 NULL，任务创建失败，运行异常
- 原因：FreeRTOS 堆（`configTOTAL_HEAP_SIZE`）不够分配
- 解决：加大 FreeRTOS 堆；加诊断代码打印 `xPortGetFreeHeapSize()`，逐个创建对象观察堆余量

**6. OLED 上浮点数的小数部分显示丢失**
- 现象：湿度只显示 `57.`，小数位和后面的 `%RH` 都不见了
- 原因：① **布局列号重叠**：后面绘制的 `%RH`/`度` 的起始列盖住了小数位的列；② `OLED_Show_Float` 未处理四舍五入进位（如 25.95 显示成 25.10）
- 解决：逐段重新计算每段内容的起始列（8 列/ASCII 字符、16 列/汉字），确保互不重叠；`Show_Float` 中当 `dec_part >= scale` 时整数部分 +1、小数部分 -scale

**7. 串口打印的"北京时间"与实际时间差 8 小时**
- 现象：实际 18:03，串口打印 10:11
- 原因：NMEA 语句输出的是 **UTC 时间**
- 解决：解析后小时数 +8（对 24 取模）换算为北京时间

**8. DHT11 读取失败，数据全 0 或校验错误**
- 现象：DHT11 偶发读取失败，或完全读不出数据
- 原因：微秒延时函数不准。TIM5 分频系数没配对，`delay_us(1)` 实际不是 1?s，单总线协议的时序全部失效
- 解决：72MHz 主频下 TIM5 的 PSC 必须设为 **71**（72MHz / (71+1) = 1MHz，1 计数 = 1?s）

**9. 低优先级任务长时间得不到运行**
- 现象：某任务饥饿，日志长时间不出现
- 原因：同优先级/高优先级任务长期占用 CPU，低优先级任务得不到调度
- 解决：GPS 任务优先级（3）必须 **不低于** DHT11 任务（2）；各任务耗时处理完及时 `vTaskDelay` 阻塞让出 CPU，不要用忙等延时

**10. 串口打印的中文是乱码**
- 现象：日志里 `湿度` 等中文字符显示乱码
- 原因：源码是 UTF-8 编码，串口助手按 GBK 解码
- 解决：串口助手接收编码选 **UTF-8**

### 硬件类

**11. 串口助手只收到一堆 `00 00 00`，没有任何有效日志**
- 现象：换串口软件后能收到数据，但内容全是 0x00
- 原因：USB-TTL 与 MCU 之间 TX/RX 没有交叉连接、没有共地，或杜邦线接触不良
- 解决：**TX→RX 交叉接线 + 必须共地**；逐根换线排除接触不良

**12. OLED 黑屏，或显示不规则乱点**
- 现象：上电后 OLED 无显示，或满屏不规则乱点
- 原因：I2C 接线（SCL/SDA）接错或接触不良；OLED 从机地址与代码不匹配；未留上电稳定时间
- 解决：核对 SCL→PB10、SDA→PB11；确认 SSD1306 地址为 0x78；初始化前加少量延时等待屏幕上电稳定

**13. 系统出现莫名中断风暴、时序紊乱**
- 现象：没接外设的引脚导致中断频繁误触发，DHT11/GPS 时序被扰乱
- 原因：UART RX 引脚悬空，电平漂移反复触发中断
- 解决：不用的引脚配置为模拟输入或下拉，不要让 RX 引脚悬空

**14. GPS 串口数据是乱码**
- 现象：USART2 收到的不是 `$GN...` 语句而是乱码
- 原因：GPS 模块默认波特率是 9600，USART2 却配成了 115200
- 解决：USART2 波特率必须与 GPS 模块一致，本项目为 **9600**

**15. GPS 经纬度始终是 `0°0'0"`，时间是 1980 年**
- 现象：解析出的 `bj=10:11:00` 时间在走但经纬度全 0，日期是 060180
- 原因：室内收不到卫星信号，模块未定位，输出的是空数据
- 解决：拿到**室外开阔地带**测试，冷启动搜星需要几分钟；未定位时显示 0 属正常

## NMEA 解析说明

解析 `$GNRMC` 语句（`strncmp` 匹配前 6 字符），按逗号分割字段后提取：

- **UTC 时间** → 北京时间（UTC+8）
- **纬度** `ddmm.mmmmm` → 度°分′秒″
- **经度** `dddmm.mmmmm` → 度°分′秒″
- 定位状态位 `A`（有效）/`V`（无效）

样例语句：

```
$GNRMC,161414.400,A,3906.71994,N,11703.86380,E,000.0,000.0,181225,,,A*71
```

## 许可证

MIT License
