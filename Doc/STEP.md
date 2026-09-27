# 从零上手：Navert 多协议通信转发仪

> **关联文档**：
> - **[README.md](../README.md)**——项目整体介绍、功能与亮点
> - **[STRUCTURE.md](STRUCTURE.md)**——工程结构、硬件架构、软件分层
> - **[STREAM.md](STREAM.md)**——数据流向、定帧机制、背压
> - 本文档是**快速上手指南**，侧重操作步骤

## 🍓前期准备

### 1. 硬件清单（必需）

| 模块 | 类型 | 说明 |
|------|-------------|------|
| **主控板** | STM32F407VET6 | 核心板 |
| **显示屏** | 2.4寸ST7789 | 240×320，SPI接口，8针（VCC/GND/SCL/SDA/RES/DC/CS/BLK） |
| **触摸屏** | FT6336 | 电容触摸 |
| **调试器** | ST-Link V2 | SWD下载（PA13/PA14） |
| **USB转TTL** | CH340 | 接USART1（PA9/PA10） |
| **杜邦线** | 若干 | 接线用 |

| 验证对象 | 设备 |
|----------|----------------|
| USART | USB转TTL |
| CAN | USB-CAN分析仪 |
| I²C | 另一块MCU |
| SPI | 逻辑分析仪 |

### 2. 验证环境

- **软件**：Keil MDK-ARM 5.36+
- **硬件**：STM32F407VET6核心板+ST7789屏（含FT6336触摸）

### 3. 获取工程

```bash
git clone <仓库地址>
```

## 💮软件配置与编译

### 1. 打开工程

1. 进入MDK-ARM/文件夹
2. 双击ST7789.uvprojx

### 2. 编译与下载

1. **编译**：点击Build（F7）
2. **下载**：点击Download（F8）
3. **复位**：按开发板复位键，程序开始运行

## 🍥运行验证

### 1. 开机

上电后屏幕应显示主界面：

```
        Navert
  CAN1    CAN2
  USART1  USART2
  IIC1    IIC2
  SPI2    SPI3
        Config
```

### 2. 快速验证

1. 串口助手接USART1，9600 8N1
2. Config页面配置CAN1为环回模式
3. 进UI的USART1页面→Forward to:选CAN1→点Start Forward
4. 串口助手发0x01：屏幕上应出现From: USART1
5. 返回进入CAN1页面：屏幕上出现From：CAN1 0x01

### 3. 在线改参数验证

1. 进某端口页面→Config
2. **串口**：改波特率到9600→保存（出现Config Saved）→用9600的串口助手仍能收到转发数据，
3. **CAN**：配置页显示的是实际位速率（如Bit Rate: 250kbps），改预分频和BS1/BS2后数字应跟着变
4. **SPI**：分频从/2改到/256→用逻辑分析仪量SCK是否跟着变
5. **输入**：Fwd CAN ID、Target Addr这类项会弹出软键盘，支持0-9 a-f A-F x X，Cancel取消，OK确认

