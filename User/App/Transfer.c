/** 
 * @file Transfer.c
 * @brief 转发实现
 * @author Nahida
 * @date 2026.9.10
 */

#include "Transfer.h"
#include "RingBuf.h"
#include <string.h>
#include "USART_RT.h"
#include "CAN_RT.h"
#include "IIC_RT.h"
#include "SPI_RT.h"
#include "ui.h"

#define TRANSFER_SRC_NUM 8

// 转发路径结构体
typedef struct
{
    Src_t des;
    uint32_t can_id;
    uint32_t can_ide;
    uint32_t can_mask;
    uint16_t iic_addr;
} TransferRoute_t;

// 为每个协议初始化一个缓冲区
RingBuf_t USART_RingBuf;
RingBuf_t CAN_RingBuf;
RingBuf_t IIC_RingBuf;
RingBuf_t SPI_RingBuf;

// 发送缓冲端口 DMA从这个数组搬运数据 有数据时置busy忙 再来数据就放hold
static Msg_t Transfer_Buf[TRANSFER_SRC_NUM];

// 转发忙 转发时置1 结束回调里清零
static volatile uint8_t tx_busy[TRANSFER_SRC_NUM] = {0};

// 路径表 决定某个端口来的数据发送到哪
static TransferRoute_t Transfer_Route[TRANSFER_SRC_NUM];

// hold位，如果目的端口忙，就暂存等不忙，与缓冲区相互配合，读缓冲区前不知道目的端口是哪，读出来后不能放回缓冲区，所以用hold位暂存
#define HOLD_COUNT 4
#define HOLD_USART 0
#define HOLD_CAN 1
#define HOLD_IIC 2
#define HOLD_SPI 3

// hold结构体 为每种协议预设hold位
static struct
{
    Msg_t msg;
    uint8_t valid;
} hold_msgs[HOLD_COUNT];

/**
  *@brief 转发初始化
  *@param NULL
  *@retval NULL
  */
void Transfer_Init(void)
{
    // 初始各通信协议缓冲区
    RingBuf_Init(&USART_RingBuf);
    RingBuf_Init(&CAN_RingBuf);
    RingBuf_Init(&IIC_RingBuf);
    RingBuf_Init(&SPI_RingBuf);

    for (int i = 0; i < TRANSFER_SRC_NUM; i++)
    {
        Transfer_Route[i].des = (Src_t)0xFF;
        Transfer_Route[i].can_id = 0;
        Transfer_Route[i].can_ide = CAN_ID_STD;
        Transfer_Route[i].can_mask = 0;
        Transfer_Route[i].iic_addr = 0;
        tx_busy[i] = 0;
    }

    for (int i = 0; i < HOLD_COUNT; i++)
        hold_msgs[i].valid = 0;
}

/**
  *@brief 发送完成 清除busy位
  *@param des 目的端口
  *@retval NULL
  */
void Transfer_TxComplete(Src_t des)
{
    if (des < TRANSFER_SRC_NUM)
        tx_busy[des] = 0;
}

/**
  *@brief 负责从hold位数据放到发送缓冲区发送 遍历四个hold位，如果有数据且对应端口不忙，就发送并清busy
  *@param NULL
  *@retval NULL
  */
static void Transfer_TryHeld(void)
{
    for (int i = 0; i < HOLD_COUNT; i++)
    {
        if (!hold_msgs[i].valid)
            continue;

        Src_t des = Transfer_Route[hold_msgs[i].msg.src].des;
        if (des <= mSPI3 && !tx_busy[des])
        {
            Transfer_Forward(&hold_msgs[i].msg);
            hold_msgs[i].valid = 0;
        }
    }
}

/**
  *@brief 负责从环形缓冲区读取数据并转发或放到hold
  *@param rb 环形缓冲区指针
  *@param hold_idx hold位索引
  *@retval NULL
  */
static void Transfer_Drain(RingBuf_t *rb, int hold_idx)
{
    if (hold_msgs[hold_idx].valid) // hold有数据
        return;

    Msg_t msg;
    if (!RingBuf_Read(rb, &msg))//环形缓冲区没有数据
        return;

    UI_OnMessage(&msg);//把数据显示到UI

    Src_t des = Transfer_Route[msg.src].des;
    if (des > mSPI3)
    {
    }
    else if (tx_busy[des])//hold没数据但是正在转发busy 就放到hold
    {
        hold_msgs[hold_idx].msg = msg;
        hold_msgs[hold_idx].valid = 1;
    }
    else
    {
        Transfer_Forward(&msg);
    }
}

/**
  *@brief 准备拿取消息转发
  *@param NULL
  *@retval NULL
  */
void Transfer_Poll(void)
{
    SPI_RT_Poll();//SPI配置为全双工 用来检测总线空闲以确定不定长数据是否发完并放到环形缓冲区

    CAN_RT_Poll();//处理总线关闭留下的复位请求 复位完这一轮就能接着转发

    //尝试转发hold的数据
    Transfer_TryHeld();

    //从环形缓冲区拿数据
    Transfer_Drain(&USART_RingBuf, HOLD_USART);
    Transfer_Drain(&CAN_RingBuf, HOLD_CAN);
    Transfer_Drain(&IIC_RingBuf, HOLD_IIC);
    Transfer_Drain(&SPI_RingBuf, HOLD_SPI);
}

/**
  *@brief 转发
  *@param msg 封装好的消息
  *@retval NULL
  */
void Transfer_Forward(Msg_t *msg)
{
    if (msg == NULL || msg->src >= TRANSFER_SRC_NUM)
        return;

    TransferRoute_t *route = &Transfer_Route[msg->src];

    if (route->des > mSPI3)
        return;

    Src_t des = route->des;

    if (tx_busy[des])//目的端口忙
        return;

    //数据拷贝到发送缓冲区并置busy等待DMA转发
    Msg_t *buf = &Transfer_Buf[des];
    buf->src = msg->src;
    buf->len = msg->len;
    memcpy(buf->data, msg->data, msg->len);

    tx_busy[des] = 1;

    HAL_StatusTypeDef hal_ret = HAL_OK;

    switch (des)
    {
    case mUSART1:
    case mUSART2:
        hal_ret = USART_RT_Send(des, buf->data, buf->len);
        break;

    case mCAN1:
    case mCAN2:
        hal_ret = CAN_RT_Send(des,Transfer_GetCANId(des),
                              Transfer_GetCANIdMode(des),
                              buf->data, buf->len);
        break;

    case mIIC1:
    case mIIC2:
        hal_ret = IIC_RT_Send(des, Transfer_GetIICAddr(des), buf->data, buf->len);
        //IIC阻塞发送，发送完就释放busy
        if (hal_ret == HAL_OK)
            Transfer_TxComplete(des);
        break;

    case mSPI2:
    case mSPI3:
        hal_ret = SPI_RT_Send(des, buf->data, buf->len);
        break;

    default:
        tx_busy[des] = 0;
        return;
    }

    if (hal_ret != HAL_OK)
        tx_busy[des] = 0;
}

/**
  *@brief 设置转发路由
  *@param src 消息来源
  *@param des 消息目的地
  *@retval NULL
  */
void Transfer_SetRoute(Src_t src, Src_t des)
{
    if (src >= TRANSFER_SRC_NUM)
        return;
    Transfer_Route[src].des = des;
}

/**
  *@brief 设置CAN目的ID
  *@param src 消息来源
  *@param can_id CAN目的ID
  *@retval NULL
  */
void Transfer_SetCANId(Src_t src, uint32_t can_id)
{
    if (src >= TRANSFER_SRC_NUM)
        return;
    Transfer_Route[src].can_id = can_id;
}

/**
  *@brief 设置CAN目的ID模式
  *@param src 消息来源
  *@param ide CAN目的ID模式
  *@retval NULL
  */
void Transfer_SetCANIdMode(Src_t src, uint32_t ide)
{
    if (src >= TRANSFER_SRC_NUM)
        return;
    Transfer_Route[src].can_ide = ide;
}

/**
  *@brief 设置CAN掩码模式
  *@param src 消息来源
  *@param mask CAN掩码模式
  *@retval NULL
  */
void Transfer_SetCANMask(Src_t src, uint32_t mask)
{
    if (src >= TRANSFER_SRC_NUM)
        return;
    Transfer_Route[src].can_mask = mask;
}

/**
  *@brief 设置IIC地址
  *@param src 消息来源
  *@param addr IIC地址
  *@retval NULL
  */
void Transfer_SetIICAddr(Src_t src, uint16_t addr)
{
    if (src >= TRANSFER_SRC_NUM)
        return;
    Transfer_Route[src].iic_addr = addr;
}

/**
  *@brief 获取CANID
  *@param src 消息来源
  *@retval CANID
  */
uint32_t Transfer_GetCANId(Src_t src)
{
    if (src >= TRANSFER_SRC_NUM)
        return 0;
    return Transfer_Route[src].can_id;
}

/**
  *@brief 获取CANID模式
  *@param src 消息来源
  *@retval CANID模式
  */
uint32_t Transfer_GetCANIdMode(Src_t src)
{
    if (src >= TRANSFER_SRC_NUM)
        return CAN_ID_STD;
    return Transfer_Route[src].can_ide;
}

/**
  *@brief 获取CAN掩码模式
  *@param src 消息来源
  *@retval CAN掩码模式
  */
uint32_t Transfer_GetCANMask(Src_t src)
{
    if (src >= TRANSFER_SRC_NUM)
        return 0;
    return Transfer_Route[src].can_mask;
}

/**
  *@brief 获取IIC目的地址
  *@param src 消息来源
  *@retval IIC目的地址
  */
uint16_t Transfer_GetIICAddr(Src_t src)
{
    if (src >= TRANSFER_SRC_NUM)
        return 0;
    return Transfer_Route[src].iic_addr;
}
