# 🍥数据流向

本文档说明**数据转发流向逻辑**

## 🍓简介

数据被拆成两个上下文处理：**中断里只负责打包写缓冲，主循环负责读缓冲转发**。
这样中断里不做长耗时的转发动作，也不会在中断里查表、开启DMA。

## 💨总览

```
中断/回调上下文：
  USART_RxEventCallback/CAN_RxFifo0MsgPending/I2C_SlaveRxCplt/SPI_RxCplt
      ->封装Msg_t（来源、长度、数据）
      ->RingBuf_Write，写进各自的环形缓冲

  USART_RingBuf/CAN_RingBuf/IIC_RingBuf/SPI_RingBuf四个独立，互不挤占
  （如USART1和USART2共用一个USART_RingBuf，靠msg.src区分具体端口）

主循环上下文：
  while(1)
      ->Transfer_Poll()
             ->SPI_RT_Poll()            //轮询BSY判断SPI从机的帧尾
             ->CAN_RT_Poll()            //处理Bus-Off留下的复位请求
             ->Transfer_CheckTimeout()  //收拾等不到完成回调的busy
             ->Transfer_TryHeld()       //补发上一轮压在hold槽里的消息
             ->Transfer_Drain()
                    ->RingBuf_Read     //取出一条Msg_t
                    ->UI_OnMessage()   //先送界面显示
                    ->查Transfer_Route[msg.src].des
                           若des无效     ->丢弃
                           若目标口忙    ->存hold槽，下一轮TryHeld再发
                           若目标口空闲  ->Transfer_Forward()
                                 ->拷贝到Transfer_Buf[des]
                                 ->tx_busy[des] = 1，tx_start[des] = HAL_GetTick()
                                 ->USART_RT_Send/CAN_RT_Send/IIC_RT_Send/SPI_RT_Send
                                        ->DMA完成中断/邮箱完成回调/阻塞发送返回
                                        ->Transfer_TxComplete(des)->tx_busy[des] = 0
      ->lv_timer_handler()  //刷 LVGL
  一帧消息转发完成。继续下一条消息接收转发
```

## 🥇一次转发的完整时序

1. 外设收完一帧→回调里封装Msg_t→RingBuf_Write()
2. Transfer_Poll()→SPI_RT_Poll()（SPI 从机判帧尾）→CAN_RT_Poll()（处理Bus-Off复位）→Transfer_CheckTimeout()（回收卡住的busy）→Transfer_TryHeld()（补发上一轮没发出去的）
3. Transfer_Drain()从Ring环形缓冲里取一条→先送界面→再查路由
4. 目标口不忙→拷进Transfer_Buf[des]→置tx_busy并记下置忙时刻→调Send()启动发送
5. 发送完成回调→Transfer_TxComplete()清tx_busy
6. 出错→错误回调→Transfer_TxComplete()放开tx_busy；CAN要是进了Bus-Off，置标志交给主循环重新初始化
7. 等不到回调→置忙超过500ms→先Transfer_AbortTx()停掉这帧→再放开tx_busy 

| 协议 | 完成回调 | 清tx_busy的位置 |
|------|----------|---------------------|
| USART | HAL_UART_TxCpltCallback | Transfer_TxComplete |
| CAN | HAL_CAN_TxMailbox0CompleteCallback等 | 邮箱完成回调里 |
| I²C | 阻塞发送 | IIC_RT_Send()返回后当场清 |
| SPI | HAL_SPI_TxCpltCallback | 回调里释放 |

## 💮为什么hold槽不能塞回环形缓冲

转发时才发现目标口还在发上一帧（DMA没完成），这时候数据已经从环形缓冲里取出来了：
此时不能放回环形缓冲区，因为写缓冲只能由接收中断完成，主循环只能读缓冲，也不能丢掉消息，
所以给每类协议留了一个hold槽暂存，下一轮Transfer_TryHeld()开头重试。**深度只有1**，因为它的意义只是忙时暂存消息，真正的缓冲在环形缓冲里还有31帧。tx_busy由发送完成回调清零，因为Send()返回OK只代表DMA开始搬运了，不代表发完了。

## 🌱相关变量定义

| 变量 | 位置 | 作用 |
|------|------|------|
| USART/CAN/IIC/SPI_RingBuf | Transfer.c | 每类协议一个接收缓冲，32槽×264B |
| Transfer_Buf[8] | Transfer.c | 每口一份发送缓冲，DMA搬运期间内容不能变 |
| tx_busy[8] | Transfer.c | 端口忙标志，置1后由发送完成回调清0 |
| tx_start[8] | Transfer.c | 每口置忙的时刻，超时兜底用（超过500ms就强撤） |
| Transfer_Route[8] | Transfer.c | 路由表 |
| hold_msgs[4] | Transfer.c | 每类协议一个hold槽，深度1 |
