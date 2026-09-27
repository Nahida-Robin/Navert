# 🏠工程结构

```
Navert/
├── Core/                          # STM32 HAL库核心文件
│   ├── Inc/                       # 头文件目录
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h   # HAL裁剪配置
│   │   ├── stm32f4xx_it.h
│   │   └── ......
│   └── Src/                       # 源文件目录
│       ├── main.c                 # 时钟168MHz、各外设Init、主循环
│       ├── stm32f4xx_hal_msp.c    # MspInit：GPIO/DMA/NVIC底层配置
│       ├── stm32f4xx_hal_timebase_tim.c  # TIM14做HAL时基
│       ├── system_stm32f4xx.c
│       └── ......
├── User/                          # 自己的代码
│   ├── App/                       # 应用层
│   │   ├── App.h                  # 端口枚举Src_t、统一消息Msg_t
│   │   ├── RingBuf.c/.h           # 单生产者单消费者环形缓冲
│   │   ├── USART_RT.c/.h          # 串口：IDLE+DMA收、DMA发、在线改波特率
│   │   ├── CAN_RT.c/.h            # CAN1/2：全通滤波器、FIFO0中断收、3个发送邮箱
│   │   ├── IIC_RT.c/.h            # I²C主/从模式切换，从机Receive_IT+错误回调
│   │   ├── SPI_RT.c/.h            # SPI2/3主/从，从机靠轮询BSY标志判帧尾
│   │   ├── Transfer.c/.h          # 转发引擎：路由表、发送缓冲、tx_busy、hold 槽
│   │   └── ui.c/.h                # LVGL界面：八口监视页+参数配置页
│   ├── Hardware/                  # 硬件驱动层
│   │   ├── LCD.c/.h               # ST7789初始化、DMA刷屏
│   │   ├── touch.c/.h / CTP.c / ctp.h    # FT6336电容触摸
│   │   ├── ctiic.c/.h             # 软件I²C
│   │   ├── FONT.H                 # 字库
│   │   └── pic.h                  # 图片素材
│   └── Sys/
│       ├── Delay.c/.h             # DWT微秒延时
│       └── Timer.c/.h             # TIM2/3定时器配置，没有参与代码
├── LVGL/                          # LVGL源码
│   ├── src/
│   ├── examples/porting/          # lv_port_disp.c/lv_port_indev.c两个移植回调
│   └── lv_conf.h                  # LVGL配置
├── Drivers/                       # 驱动层
│   ├── CMSIS/                     # ARM CMSIS核心
│   └── STM32F4xx_HAL_Driver/      # STM32 HAL驱动库
├── MDK-ARM/                       # Keil MDK工程文件
│   ├── DebugConfig/
│   ├── RTE/
│   └── ST7789.uvprojx             # 工程文件
├── Doc/                           # 项目文档
│   ├── STRUCTURE.md               # 本文档 工程结构
│   ├── STREAM.md                  # 数据流向
│   └── STEP.md                    # 快速上手指南
│
├──DataSheet/                      #各模块、芯片数据手册
├── ST7789.ioc                     # STM32CubeMX配置文件
└── README.md                      # 整体介绍
```


# 🎨硬件架构

## 主控芯片

- **MCU**：STM32F407VET6
- **时钟**：HSE 12MHz 主频168MHz

## 外设模块

| 模块 | 类型 | 接口 |
|------|-------------|------|
| 串口 1 | USART1 | PA9(TX)/PA10(RX) |
| 串口 2 | USART2 | PA2(TX)/PA3(RX) |
| CAN 1 | CAN1 | PA11(RX)/PA12(TX) |
| CAN 2 | CAN2 | PB12(RX)/PB6(TX) |
| I²C 1 | I2C1 | PB8(SCL)/PB7(SDA) |
| I²C 2 | I2C2 | PB10(SCL)/PB11(SDA) |
| SPI 2 | SPI2 | PB13(SCK)/PB14(MISO)/PB15(MOSI) |
| SPI 3 | SPI3 | PC10(SCK)/PC11(MISO)/PC12(MOSI) |
| 显示屏(SPI1) | ST7789 240×320 | PE0(SCK)/PE1(MISO)/PE2(MOSI)/PE3(RST)/PE4(DC)/PE5(CS)/PE6(LED) |
| 触摸(软件IIC) | FT6336电容触摸 | PA4(TSDI)/PA5(TIRQ)/PA6(TCLK)/PA7(TCS) |
| 时基 | TIM14 | 内部 |
| 调试 | SWD | PA13/PA14 |


# 🍥软件架构

## 应用层模块

| 文件 | 作用 |
|------|------|
| App.h | 端口枚举Src_t、统一消息Msg_t |
| RingBuf.c | 单生产者单消费者环形缓冲 |
| USART_RT.c | 串口：IDLE+DMA收，DMA发，支持在线改参数 |
| CAN_RT.c | CAN1/2：全通滤波器、FIFO0中断收、3个发送邮箱 |
| IIC_RT.c | I²C主/从模式切换，从机用Receive_IT+错误回调 |
| SPI_RT.c | SPI2/3主/从，从机靠轮询BSY标志判帧尾 |
| Transfer.c | 转发引擎：路由表、发送缓冲、tx_busy、hold槽 |
| ui.c | LVGL页面：八个口的收发监视+参数配置页 |

## 核心数据结构

```c
//端口名枚举：数组索引就是端口号
typedef enum {
	mCAN1 = 0, mCAN2, mUSART1, mUSART2,
	mIIC1,     mIIC2, mSPI2,   mSPI3
} Src_t;

//统一消息封装
typedef struct {
	Src_t    src;
	uint16_t len;
	uint8_t  data[256];
} Msg_t;

//路由表：一项对应一个源端口，des决定去哪
typedef struct {
	Src_t    des;        //目标端口
	uint32_t can_id;     //转发到CAN时用的ID
	uint32_t can_ide;    //标准帧/扩展帧
	uint32_t can_mask;   //CAN掩码
	uint16_t iic_addr;   //转发到I²C时的从机地址
} TransferRoute_t;

//转发引擎
RingBuf_t  USART_RingBuf, CAN_RingBuf, IIC_RingBuf, SPI_RingBuf;  //每类协议一个，32×264B
static Msg_t Transfer_Buf[8];        //每口一份静态发送缓冲
static volatile uint8_t tx_busy[8];  //每口忙标志，发送完成回调里清零
static struct { Msg_t msg; uint8_t valid; } hold_msgs[4];  //每类协议一个hold槽
```

## 几个核心

**统一消息**Msg_t{src, len, data[256]}
所有协议收上来的数据都打包成它再进缓冲，转发引擎只看Msg_t和路由表

**环形缓冲按协议类型分，不是按端口分**
USART1/2共用一个USART_RingBuf，靠Msg_t.src区分具体是哪个口。这样缓冲和接收回调一一对应

**驱动接口统一**
Transfer_Forward()只按目标口分派Send，协议之间的参数差异（CAN的ID/IDE、I²C的从机地址）
存在路由表里，发送时再取出来用。所以switch(des)里各分支拿到的都是同一套入参

## 资源占用

Flash**228.46KB/512KB**，RAM（RW+ZI）**102.31KB/128KB**。
RAM大头是4个环形缓冲（约33KB）、LVGL堆（2KB）和LVGL双缓冲（各15行，合计约14KB）。
