/**
 * @file SPI_RT.c
 * @brief SPI配置
 * @author Nahida
 * @date 2026.9.10
 */

#include "SPI_RT.h"
#include "Transfer.h"
#include <string.h>

//RX接收缓冲区
static uint8_t SPI2_RxBuf[SPI_RX_BUF_SIZE];
static uint8_t SPI3_RxBuf[SPI_RX_BUF_SIZE];

//不定长数据接收完成标志位
static uint8_t spi2_slave_bsy_ok;
static uint8_t spi3_slave_bsy_ok;

/**
  *@brief 关闭SPI2/SPI3的NVIC中断
  *@param NULL
  *@retval NULL
  */
static void SPI_NVIC_Disable(void)
{
    HAL_NVIC_DisableIRQ(SPI2_IRQn);
    HAL_NVIC_DisableIRQ(SPI3_IRQn);
}

/**
  *@brief SPI初始化
  *@param NULL
  *@retval NULL
  */
void SPI_RT_Init(void)
{
    HAL_SPI_Abort(&hspi2);
    HAL_SPI_Abort(&hspi3);

    //关闭SPI2/3中断
    SPI_NVIC_Disable();
}

/**
  *@brief SPI软件复位
  *@param hspi SPI句柄
  *@retval NULL
  */
static void SPI_Full_Reset(SPI_HandleTypeDef *hspi)
{
    HAL_SPI_Abort(hspi);

    HAL_SPI_Init(hspi);

    SPI_NVIC_Disable();
}

/**
  *@brief SPI配置
  *@param des SPI目标端口
  *@param cfg SPI配置参数结构体
  *@retval NULL
  */
void SPI_RT_Config(Src_t des, SPI_Config_t *cfg)
{
    if (cfg == NULL) return;

    SPI_HandleTypeDef *hspi;

    switch (des)
    {
        case mSPI2: hspi = &hspi2; break;
        case mSPI3: hspi = &hspi3; break;
        default: return;
    }

    HAL_SPI_DeInit(hspi);
    hspi->Init.Mode              = cfg->Mode;
    hspi->Init.Direction         = cfg->Direction;
    hspi->Init.DataSize          = cfg->DataSize;
    hspi->Init.CLKPolarity       = cfg->CLKPolarity;
    hspi->Init.CLKPhase          = cfg->CLKPhase;
    hspi->Init.NSS               = cfg->NSS;
    hspi->Init.BaudRatePrescaler = cfg->BaudRatePrescaler;
    hspi->Init.FirstBit          = cfg->FirstBit;
    HAL_SPI_Init(hspi);

    SPI_NVIC_Disable();

    GPIO_InitTypeDef gpio = {
        .Mode   = GPIO_MODE_AF_PP,
        .Pull   = GPIO_NOPULL,
        .Speed  = GPIO_SPEED_FREQ_LOW,
    };

    if (cfg->BaudRatePrescaler <= SPI_BAUDRATEPRESCALER_4)
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    else if (cfg->BaudRatePrescaler <= SPI_BAUDRATEPRESCALER_16)
        gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;

    switch (des)
    {
        case mSPI2:
            gpio.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
            gpio.Alternate = GPIO_AF5_SPI2;
            HAL_GPIO_Init(GPIOB, &gpio);
            break;
        case mSPI3:
            gpio.Pin       = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
            gpio.Alternate = GPIO_AF6_SPI3;
            HAL_GPIO_Init(GPIOC, &gpio);
            break;
        default:
            break;
    }

    HAL_DMA_DeInit(hspi->hdmatx);
    hspi->hdmatx->Init.FIFOMode      = DMA_FIFOMODE_DISABLE;
    hspi->hdmatx->Init.Priority      = DMA_PRIORITY_MEDIUM;
    HAL_DMA_Init(hspi->hdmatx);

    HAL_DMA_DeInit(hspi->hdmarx);
    hspi->hdmarx->Init.FIFOMode      = DMA_FIFOMODE_DISABLE;
    hspi->hdmarx->Init.Priority      = DMA_PRIORITY_MEDIUM;
    HAL_DMA_Init(hspi->hdmarx);
}

/**
  *@brief 配置接收
  *@param des SPI目标端口
  *@retval NULL
  */
void SPI_RT_EnterSlaveMode(Src_t des)
{
    SPI_HandleTypeDef *hspi;
    uint8_t *rx_buf;

    switch (des)
    {
        case mSPI2: hspi = &hspi2; rx_buf = SPI2_RxBuf; break;
        case mSPI3: hspi = &hspi3; rx_buf = SPI3_RxBuf; break;
        default: return;
    }

    //从机模式
    hspi->Init.Mode = SPI_MODE_SLAVE;
    SPI_Full_Reset(hspi);

    if (des == mSPI2) { spi2_slave_bsy_ok = 0; }
    else               { spi3_slave_bsy_ok = 0; }

    HAL_SPI_Receive_DMA(hspi, rx_buf, SPI_RX_BUF_SIZE);
}

/**
  *@brief 配置发送
  *@param des SPI目标端口
  *@retval NULL
  */
void SPI_RT_EnterMasterMode(Src_t des)
{
    SPI_HandleTypeDef *hspi;

    switch (des)
    {
        case mSPI2: hspi = &hspi2; break;
        case mSPI3: hspi = &hspi3; break;
        default: return;
    }

    //主机模式
    hspi->Init.Mode = SPI_MODE_MASTER;
    SPI_Full_Reset(hspi);

    if (des == mSPI2) { spi2_slave_bsy_ok = 0; }
    else               { spi3_slave_bsy_ok = 0; }
}

/**
  *@brief 不定长接收轮询
  *@note 此函数在Transfer_Poll()中被循环调用，通过检测SPIBSY标志判断是否接收完成
  *@param NULL
  *@retval NULL
  */
void SPI_RT_Poll(void)
{
    //SPI2从机
    if (hspi2.Init.Mode == SPI_MODE_SLAVE && hspi2.State == HAL_SPI_STATE_BUSY_RX)
    {
        uint16_t ndtr   = __HAL_DMA_GET_COUNTER(hspi2.hdmarx);
        uint16_t rx_cnt = SPI_RX_BUF_SIZE - ndtr;

        if (rx_cnt == 0)
        {
            spi2_slave_bsy_ok = 0;
        }
        else if (__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_BSY))
        {
            //正在接收中，不清零计数
        }
        else
        {
            if (++spi2_slave_bsy_ok >= 2)
            {
                spi2_slave_bsy_ok = 0;

                HAL_SPI_Abort(&hspi2);

                Msg_t msg;
                memset(&msg, 0, sizeof(msg));
                msg.src   = mSPI2;
                msg.len   = rx_cnt;
                memcpy(msg.data, SPI2_RxBuf, rx_cnt);
                RingBuf_Write(&SPI_RingBuf, &msg);

                //重新启动从机接收DMA，等待下一帧外部主机的SCK
                HAL_SPI_Receive_DMA(&hspi2, SPI2_RxBuf, SPI_RX_BUF_SIZE);
            }
        }
    }

    //SPI3从机
    if (hspi3.Init.Mode == SPI_MODE_SLAVE && hspi3.State == HAL_SPI_STATE_BUSY_RX)
    {
        uint16_t ndtr   = __HAL_DMA_GET_COUNTER(hspi3.hdmarx);
        uint16_t rx_cnt = SPI_RX_BUF_SIZE - ndtr;

        if (rx_cnt == 0)
        {
            spi3_slave_bsy_ok = 0;
        }
        else if (__HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_BSY))
        {
            //正在传输中
        }
        else
        {
            if (++spi3_slave_bsy_ok >= 2)
            {
                spi3_slave_bsy_ok = 0;

                HAL_SPI_Abort(&hspi3);

                Msg_t msg;
                memset(&msg, 0, sizeof(msg));
                msg.src   = mSPI3;
                msg.len   = rx_cnt;
                memcpy(msg.data, SPI3_RxBuf, rx_cnt);
                RingBuf_Write(&SPI_RingBuf, &msg);

                HAL_SPI_Receive_DMA(&hspi3, SPI3_RxBuf, SPI_RX_BUF_SIZE);
            }
        }
    }
}

/**
  *@brief 单向发送
  *@param des SPI目标端口
  *@param data 待发送数据缓冲区
  *@param len 待发送数据长度
  *@retval
  */
HAL_StatusTypeDef SPI_RT_Send(Src_t des, uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return HAL_ERROR;

    SPI_HandleTypeDef *hspi;

    switch (des)
    {
        case mSPI2: hspi = &hspi2; break;
        case mSPI3: hspi = &hspi3; break;
        default: return HAL_ERROR;
    }

    return HAL_SPI_Transmit_DMA(hspi, data, len);
}

/**
  *@brief 接收完成回调 主机TX完成回调已在LCD.c中实现
  *@param hspi SPI句柄
  *@retval NULL
  */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Init.Mode != SPI_MODE_SLAVE) return;

    Msg_t msg;
    memset(&msg, 0, sizeof(msg));

    uint8_t *rx_buf;

    if (hspi->Instance == SPI2)
    {
        msg.src   = mSPI2;
        rx_buf    = SPI2_RxBuf;
    }
    else if (hspi->Instance == SPI3)
    {
        msg.src   = mSPI3;
        rx_buf    = SPI3_RxBuf;
    }
    else
    {
        return;
    }

    //获取实际接收长度
    uint16_t rx_cnt = SPI_RX_BUF_SIZE - (uint16_t)__HAL_DMA_GET_COUNTER(hspi->hdmarx);
    if (rx_cnt == 0) rx_cnt = SPI_RX_BUF_SIZE;

    msg.len = (rx_cnt <= sizeof(msg.data)) ? rx_cnt : sizeof(msg.data);
    memcpy(msg.data, rx_buf, msg.len);

    //重新启动从机接收DMA，等待下一帧外部主机的SCK
    HAL_SPI_Receive_DMA(hspi, rx_buf, SPI_RX_BUF_SIZE);

    RingBuf_Write(&SPI_RingBuf, &msg);
}
