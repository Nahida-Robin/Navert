/** 
 * @file USART_RT.c
 * @brief USART配置
 * @author Nahida
 * @date 2026.9.10
 */

#include "USART_RT.h"
#include "Transfer.h"
#include <string.h>
#include "stm32f4xx_hal.h"

//UART接收缓冲区
static uint8_t USART1_RX_Buf[64];
static uint8_t USART2_RX_Buf[64];

/**
  *@brief 重启空闲中断DMA搬运
  *@param huart UART句柄
  *@param buf 接收缓冲区
  *@param len 缓冲区长度
  *@retval NULL
  */
static void USART_Restart_IDLE_DMA(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t len)
{
    HAL_UART_AbortReceive(huart);
    HAL_UARTEx_ReceiveToIdle_DMA(huart, buf, len);
}

/**
  *@brief 把RxBuf接收的数据打包成Msg格式并写入环形缓冲区
  *@param huart UART句柄
  *@param Size 接收数据长度
  *@param rx_buf 接收缓冲区
  *@param ring_buf 环形缓冲区
  *@retval NULL
  */
static void USART_Process_Received(UART_HandleTypeDef *huart, uint16_t Size,
                                   uint8_t *rx_buf, RingBuf_t *ring_buf)
{
    if (Size == 0) return;

    Msg_t msg;
    memset(&msg, 0, sizeof(msg));

    if (huart->Instance == USART1)
        msg.src = mUSART1;
    else if (huart->Instance == USART2)
        msg.src = mUSART2;
    else
        return;

    msg.len = (Size < sizeof(msg.data)) ? Size : sizeof(msg.data);
    memcpy(msg.data, rx_buf, msg.len);

    RingBuf_Write(ring_buf, &msg);
}

/**
  *@brief 初始化 启动DMA接收空闲中断
  *@param NULL
  *@retval NULL
  */
void USART_RT_Init(void)
{
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, USART1_RX_Buf, 64);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, USART2_RX_Buf, 64);
}

/**
  *@brief UART配置设置
  *@param des 目标端口
  *@param cfg 配置参数
  *@retval NULL
  */
void USART_RT_Config(Src_t des, USART_Config_t *cfg)
{
    if (cfg == NULL) return;

    UART_HandleTypeDef *huart;
    uint8_t *rx_buf;

    switch (des)
    {
        case mUSART1: huart = &huart1; rx_buf = USART1_RX_Buf; break;
        case mUSART2: huart = &huart2; rx_buf = USART2_RX_Buf; break;
        default: return;
    }

    HAL_UART_DMAStop(huart);//先停止DMA

    huart->Init.BaudRate   = cfg->BaudRate;
    huart->Init.WordLength = cfg->WordLength;
    huart->Init.StopBits   = cfg->StopBits;
    huart->Init.Parity     = cfg->Parity;
    HAL_UART_Init(huart);//重新初始化

        USART_Restart_IDLE_DMA(huart, rx_buf, 64);
}

/**
  *@brief UART发送数据
  *@param des 目标端口
  *@param data 数据缓冲区
  *@param len 数据长度
  *@retval HAL_StatusTypeDef 发送状态
  */
HAL_StatusTypeDef USART_RT_Send(Src_t des, uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return HAL_ERROR;

    switch (des)
    {
        case mUSART1:
            return HAL_UART_Transmit_DMA(&huart1, data, len);
        case mUSART2:
            return HAL_UART_Transmit_DMA(&huart2, data, len);
        default:
            return HAL_ERROR;
    }
}

/**
  *@brief 空闲回调
  *@param huart UART句柄
  *@param Size 接收数据长度
  *@retval NULL
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        USART_Process_Received(huart, Size, USART1_RX_Buf, &USART_RingBuf);
        USART_Restart_IDLE_DMA(&huart1, USART1_RX_Buf, 64);
    }
    else if (huart->Instance == USART2)
    {
        USART_Process_Received  (huart, Size, USART2_RX_Buf, &USART_RingBuf);
        USART_Restart_IDLE_DMA(&huart2, USART2_RX_Buf, 64);
    }
}

/**
  *@brief 收满回调
  *@param huart UART句柄
  *@retval NULL
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        USART_Process_Received(huart, 64, USART1_RX_Buf, &USART_RingBuf);
        USART_Restart_IDLE_DMA(&huart1, USART1_RX_Buf, 64);
    }
    else if (huart->Instance == USART2)
    {
        USART_Process_Received(huart, 64, USART2_RX_Buf, &USART_RingBuf);
        USART_Restart_IDLE_DMA(&huart2, USART2_RX_Buf, 64);
    }
}

/**
  *@brief 错误回调
  *@param huart UART句柄
  *@retval NULL
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        Transfer_TxComplete(mUSART1);//清除busy标志位
        USART_Restart_IDLE_DMA(&huart1, USART1_RX_Buf, 64);
    }
    else if (huart->Instance == USART2)
    {
        Transfer_TxComplete(mUSART2);
        USART_Restart_IDLE_DMA(&huart2, USART2_RX_Buf, 64);
    }
}

/**
  *@brief DMA啊发送完成回调
  *@param huart UART句柄
  *@retval NULL
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        Transfer_TxComplete(mUSART1);
    }
    else if (huart->Instance == USART2)
    {
        Transfer_TxComplete(mUSART2);
    }
}
