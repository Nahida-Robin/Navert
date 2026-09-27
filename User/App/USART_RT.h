#ifndef __USART_RT_H
#define __USART_RT_H

#include "App.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

#define USART_RX_BUF_SIZE   64

//串口配置结构体
typedef struct {
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
} USART_Config_t;

void USART_RT_Init(void);
void USART_RT_Config(Src_t des, USART_Config_t *cfg);
HAL_StatusTypeDef USART_RT_Send(Src_t des, uint8_t *data, uint16_t len);

#endif
