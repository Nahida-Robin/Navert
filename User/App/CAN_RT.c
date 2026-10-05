/** 
 * @file CAN_RT.c
 * @brief CAN配置
 * @author Nahida
 * @date 2026.9.10
 */

#include "CAN_RT.h"
#include "Transfer.h"
#include <string.h>

//CAN接收缓冲区
static uint8_t CAN1_RxBuf[8];
static uint8_t CAN2_RxBuf[8];

//总线关闭标志
static volatile uint8_t can1_busoff;
static volatile uint8_t can2_busoff;

/**
  *@brief CAN使能
  *@param hcan CAN句柄
  *@retval NULL
  */
static void CAN_Start(CAN_HandleTypeDef *hcan)
{
    CAN_FilterTypeDef filter = {
        .FilterIdHigh       = 0,
        .FilterIdLow        = 0,
        .FilterMaskIdHigh   = 0,//掩码全为0 即全接收
        .FilterMaskIdLow    = 0,
        .FilterMode         = CAN_FILTERMODE_IDMASK,
        .FilterScale        = CAN_FILTERSCALE_16BIT,
        .FilterActivation   = CAN_FILTER_ENABLE,
        .SlaveStartFilterBank = 14,
    };

    if (hcan->Instance == CAN1)
    {
        filter.FilterBank = 0;
        filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    }
    else
    {
        filter.FilterBank = 14;
        filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    }

    HAL_CAN_ConfigFilter(hcan, &filter);
    HAL_CAN_Start(hcan);
    //接收/发送完成+错误类通知
    HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY |
                                       CAN_IT_ERROR | CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE |
                                       CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE); //使能CAN外设中断通知
}

/**
  *@brief CAN初始化
  *@param NULL
  *@retval NULL
  */
void CAN_RT_Init(void)
{
    CAN_Start(&hcan1);
    CAN_Start(&hcan2);
}

/**
  *@brief CAN总线关闭后的复位
  *@param hcan CAN句柄
  *@retval NULL
  */
static void CAN_Recover(CAN_HandleTypeDef *hcan)
{
    HAL_CAN_Stop(hcan);
    HAL_CAN_Init(hcan);
    HAL_CAN_AbortTxRequest(hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);

    CAN_Start(hcan);
}

/**
  *@brief CAN配置
  *@param des CAN目标端口
  *@param cfg CAN配置结构体指针
  *@retval NULL
  */
void CAN_RT_Config(Src_t des, CAN_Config_t *cfg)
{
    if (cfg == NULL) return;

    CAN_HandleTypeDef *hcan;

    switch (des)
    {
        case mCAN1: hcan = &hcan1; break;
        case mCAN2: hcan = &hcan2; break;
        default: return;
    }

    //先停止CAN，再更新配置
    HAL_CAN_Stop(hcan);

    hcan->Init.Prescaler          = cfg->Prescaler;
    hcan->Init.Mode               = cfg->Mode;
    hcan->Init.SyncJumpWidth      = cfg->SyncJumpWidth;
    hcan->Init.TimeSeg1           = cfg->TimeSeg1;
    hcan->Init.TimeSeg2           = cfg->TimeSeg2;
    hcan->Init.AutoBusOff         = cfg->AutoBusOff;
    hcan->Init.AutoRetransmission = cfg->AutoRetransmission;

    hcan->State = HAL_CAN_STATE_READY;
    HAL_CAN_Init(hcan);

    CAN_Start(hcan);
}

/**
  *@brief CAN发送
  *@param des CAN目标端口
  *@param id CAN ID
  *@param ide CAN ID类型（标准/扩展）
  *@param data 待发送数据缓冲区
  *@param len 待发送数据长度
  *@retval HAL_StatusTypeDef
  */
HAL_StatusTypeDef CAN_RT_Send(Src_t des, uint32_t id, uint32_t ide, uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return HAL_ERROR;

    CAN_HandleTypeDef *hcan;

    switch (des)
    {
        case mCAN1: hcan = &hcan1; break;
        case mCAN2: hcan = &hcan2; break;
        default: return HAL_ERROR;
    }

    CAN_TxHeaderTypeDef txHeader = {
        .StdId             = (ide == CAN_ID_EXT) ? 0 : id,
        .ExtId             = (ide == CAN_ID_EXT) ? id : 0,
        .IDE               = ide,
        .RTR               = CAN_RTR_DATA,
        .DLC               = len > 8 ? 8 : len,
        .TransmitGlobalTime = DISABLE,
    };

    uint32_t txMailbox;
    return HAL_CAN_AddTxMessage(hcan, &txHeader, data, &txMailbox);
}

/**
  *@brief 撤掉待发的邮箱 超时用
  *@param des CAN目标端口
  *@retval NULL
  */
void CAN_RT_AbortTx(Src_t des)
{
    CAN_HandleTypeDef *hcan;

    switch (des)
    {
        case mCAN1: hcan = &hcan1; break;
        case mCAN2: hcan = &hcan2; break;
        default: return;
    }

    HAL_CAN_AbortTxRequest(hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
}

/**
  *@brief CAN接收FIFO0消息待处理回调函数
  *@param hcan CAN句柄
  *@retval NULL
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    Msg_t msg;
    uint8_t canData[8];
    uint32_t id;

    memset(&msg, 0, sizeof(msg));

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, canData);

    if (hcan->Instance == CAN1)
        msg.src = mCAN1;
    else if (hcan->Instance == CAN2)
        msg.src = mCAN2;
    else
        return;

    id = (rxHeader.IDE == CAN_ID_STD) ? rxHeader.StdId : rxHeader.ExtId;

    msg.data[0] = id & 0xFF;
    msg.data[1] = (id >> 8) & 0xFF;
    msg.data[2] = (id >> 16) & 0xFF;
    msg.data[3] = (id >> 24) & 0xFF;
    msg.data[4] = rxHeader.DLC;
    memcpy(&msg.data[5], canData, rxHeader.DLC);
    msg.len = 5 + rxHeader.DLC;

    RingBuf_Write(&CAN_RingBuf, &msg);
}

/**
  *@brief 发送完成回调 通知上层并清除busy标志
  *@param hcan CAN句柄
  *@retval NULL
  */
static void CAN_TX_Complete(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
        Transfer_TxComplete(mCAN1);
    else if (hcan->Instance == CAN2)
        Transfer_TxComplete(mCAN2);
}

/**
  *@brief CAN轮询 处理中断里攒下的总线关闭复位请求
  *@param NULL
  *@retval NULL
  */
void CAN_RT_Poll(void)
{
    if (can1_busoff)
    {
        can1_busoff = 0;
        CAN_Recover(&hcan1);
    }
    
    if (can2_busoff)
    {
        can2_busoff = 0;
        CAN_Recover(&hcan2);
    }
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) { CAN_TX_Complete(hcan); }
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) { CAN_TX_Complete(hcan); }
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) { CAN_TX_Complete(hcan); }

/**
  *@brief CAN错误回调
  *@param hcan CAN句柄
  *@retval NULL
  */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
        Transfer_TxComplete(mCAN1);
    else if (hcan->Instance == CAN2)
        Transfer_TxComplete(mCAN2);
    else
        return;

    //总线关闭标志
    if ((hcan->Instance->ESR & CAN_ESR_BOFF) != 0U)
    {
        if (hcan->Instance == CAN1)
            can1_busoff = 1;
        else
            can2_busoff = 1;
    }
}

