#ifndef __SPI_RT_H
#define __SPI_RT_H

#include "App.h"

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;

#define SPI_RX_BUF_SIZE   256

//SPI配置结构体
typedef struct {
    uint32_t Mode;
    uint32_t Direction;
    uint32_t DataSize;
    uint32_t CLKPolarity;
    uint32_t CLKPhase;
    uint32_t NSS;
    uint32_t BaudRatePrescaler;
    uint32_t FirstBit;
} SPI_Config_t;

void SPI_RT_Init(void);
void SPI_RT_Config(Src_t des, SPI_Config_t *cfg);
void SPI_RT_EnterSlaveMode(Src_t des);
void SPI_RT_EnterMasterMode(Src_t des);
void SPI_RT_Poll(void);
HAL_StatusTypeDef SPI_RT_Send(Src_t des, uint8_t *data, uint16_t len);

#endif
