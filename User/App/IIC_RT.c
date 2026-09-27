/** 
 * @file IIC_RT.c
 * @brief IIC配置
 * @author Nahida
 * @date 2026.9.10
 */

#include "IIC_RT.h"
#include "Transfer.h"
#include <string.h>

//IIC接收缓冲区
static uint8_t IIC1_RxBuf[256];
static uint8_t IIC2_RxBuf[256];

/**
  *@brief 禁用IIC中断，保护主机阻塞发送
  *@param hi2c IIC句柄
  *@retval NULL
  */
static void IIC_IRQ_Disable(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
        HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
    }
    else if (hi2c->Instance == I2C2)
    {
        HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);
    }
}

/**
  *@brief 重新使能IIC事件/错误中断
  *@param hi2c IIC句柄
  *@retval NULL
  */
static void IIC_IRQ_Enable(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
        HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
    }
    else if (hi2c->Instance == I2C2)
    {
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
    }
}

/**
  *@brief 重启IIC
  *@param hi2c IIC句柄
  *@retval NULL
  */
static void IIC_Full_Reset(I2C_HandleTypeDef *hi2c)
{
    HAL_I2C_DeInit(hi2c);
    HAL_I2C_Init(hi2c);

    IIC_IRQ_Disable(hi2c);
}

//保留为空
void IIC_RT_Init(void)
{
}

/**
  *@brief 配置IIC的参数
  *@param des 目标IIC端口
  *@param cfg 配置参数结构体指针
  *@retval NULL
  */
void IIC_RT_Config(Src_t des, IIC_Config_t *cfg)
{
    if (cfg == NULL) return;

    I2C_HandleTypeDef *hi2c;

    switch (des)
    {
        case mIIC1: hi2c = &hi2c1; break;
        case mIIC2: hi2c = &hi2c2; break;
        default: return;
    }

    hi2c->Init.ClockSpeed      = cfg->ClockSpeed;
    hi2c->Init.DutyCycle       = cfg->DutyCycle;
    hi2c->Init.OwnAddress1     = cfg->OwnAddress1;
    hi2c->Init.AddressingMode  = cfg->AddressingMode;
    hi2c->Init.DualAddressMode = cfg->DualAddressMode;
    hi2c->Init.OwnAddress2     = cfg->OwnAddress2;
    hi2c->Init.GeneralCallMode = cfg->GeneralCallMode;
    hi2c->Init.NoStretchMode   = cfg->NoStretchMode;

    IIC_Full_Reset(hi2c);
}

/**
  *@brief 从机接收模式
  *@param des 目标IIC端口
  *@retval NULL
  */
void IIC_RT_EnterSlaveMode(Src_t des)
{
    I2C_HandleTypeDef *hi2c;
    uint8_t *buf;

    switch (des)
    {
        case mIIC1: hi2c = &hi2c1; buf = IIC1_RxBuf; break;
        case mIIC2: hi2c = &hi2c2; buf = IIC2_RxBuf; break;
        default: return;
    }

    IIC_Full_Reset(hi2c);
    IIC_IRQ_Enable(hi2c);
    HAL_I2C_Slave_Receive_IT(hi2c, buf, 256);
}

/**
  *@brief 主机发送模式
  *@param des 目标IIC端口
  *@retval NULL
  */
void IIC_RT_EnterMasterMode(Src_t des)
{
    I2C_HandleTypeDef *hi2c;

    switch (des)
    {
        case mIIC1: hi2c = &hi2c1; break;
        case mIIC2: hi2c = &hi2c2; break;
        default: return;
    }

    IIC_Full_Reset(hi2c);
    IIC_IRQ_Disable(hi2c);
}

/**
  *@brief 主机发送数据
  *@param des 目标IIC端口
  *@param devAddr 目标设备地址
  *@param data 数据缓冲区
  *@param len 数据长度
  *@retval HAL_StatusTypeDef 发送状态
  */
HAL_StatusTypeDef IIC_RT_Send(Src_t des, uint16_t devAddr, uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return HAL_ERROR;

    I2C_HandleTypeDef *hi2c;

    switch (des)
    {
        case mIIC1: hi2c = &hi2c1; break;
        case mIIC2: hi2c = &hi2c2; break;
        default: return HAL_ERROR;
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(hi2c, devAddr << 1, data, len, 100);

    if (ret == HAL_BUSY)
    {
        IIC_Full_Reset(hi2c);
        ret = HAL_I2C_Master_Transmit(hi2c, devAddr << 1, data, len, 100);
    }

    return ret;
}

/**
  *@brief 作为从机接收回调
  *@param hi2c IIC句柄
  *@retval NULL
  */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    Msg_t msg;
    memset(&msg, 0, sizeof(msg));

    uint8_t *rx_buf;

    if (hi2c->Instance == I2C1)
    {
        msg.src = mIIC1;
        rx_buf = IIC1_RxBuf;
    }
    else if (hi2c->Instance == I2C2)
    {
        msg.src = mIIC2;
        rx_buf = IIC2_RxBuf;
    }
    else
    {
        return;
    }

    //计算实际接收长度
    uint16_t actual = IIC_RX_BUF_SIZE - hi2c->XferCount;
    if (actual > sizeof(msg.data))
        actual = sizeof(msg.data);

    msg.len = actual;
    memcpy(msg.data, rx_buf, msg.len);
    RingBuf_Write(&IIC_RingBuf, &msg);
    IIC_RT_EnterSlaveMode(msg.src);
}

/**
  *@brief 接收不定长数据回调
  *@param hi2c IIC句柄
  *@retval NULL
  */
static void IIC_Process_Error(I2C_HandleTypeDef *hi2c)
{
    uint8_t *rx_buf;
    Src_t src;

    if (hi2c->Instance == I2C1) { src = mIIC1; rx_buf = IIC1_RxBuf; }
    else if (hi2c->Instance == I2C2) { src = mIIC2; rx_buf = IIC2_RxBuf; }
    else return;


    uint16_t actual = IIC_RX_BUF_SIZE - hi2c->XferCount;
    if (hi2c->XferCount == 0)
    {
        uint16_t pbuf_off = (uint16_t)(hi2c->pBuffPtr - rx_buf);
        if (pbuf_off == 0)
        {
            IIC_RT_EnterSlaveMode(src);
            return;
        }
        actual = IIC_RX_BUF_SIZE;
    }

    if (actual > IIC_RX_BUF_SIZE)
        actual = IIC_RX_BUF_SIZE;

    if (actual > 0)
    {
        Msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.src  = src;
        msg.len  = (actual <= sizeof(msg.data)) ? actual : sizeof(msg.data);
        memcpy(msg.data, rx_buf, msg.len);
        RingBuf_Write(&IIC_RingBuf, &msg);
    }

    IIC_RT_EnterSlaveMode(src);
}

/**
  *@brief 错误回调
  *@param hi2c IIC句柄
  *@retval NULL
  */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    IIC_Process_Error(hi2c);
}

/**
  *@brief 接收满回调
  *@param hi2c IIC句柄
  *@retval NULL 
  */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    uint8_t *rx_buf;
    Src_t src;

    if (hi2c->Instance == I2C1)
    {
        src    = mIIC1;
        rx_buf = IIC1_RxBuf;
    }
    else if (hi2c->Instance == I2C2)
    {
        src    = mIIC2;
        rx_buf = IIC2_RxBuf;
    }
    else
    {
        return;
    }

    uint16_t actual = (uint16_t)(hi2c->pBuffPtr - rx_buf);
    if (actual > IIC_RX_BUF_SIZE)
        actual = IIC_RX_BUF_SIZE;

    if (actual > 0)
    {
        Msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.src  = src;
        msg.len  = actual;
        memcpy(msg.data, rx_buf, actual);
        RingBuf_Write(&IIC_RingBuf, &msg);
    }

    IIC_RT_EnterSlaveMode(src);
}
