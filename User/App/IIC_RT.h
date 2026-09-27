#ifndef __IIC_RT_H
#define __IIC_RT_H

#include "App.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

#define IIC_RX_BUF_SIZE   256

//IIC配置结构体
typedef struct {
    uint32_t ClockSpeed;
    uint32_t DutyCycle;
    uint32_t OwnAddress1;
    uint32_t AddressingMode;
    uint32_t DualAddressMode;
    uint32_t OwnAddress2;
    uint32_t GeneralCallMode;
    uint32_t NoStretchMode;
} IIC_Config_t;

void IIC_RT_Init(void);
void IIC_RT_Config(Src_t des, IIC_Config_t *cfg);
void IIC_RT_EnterSlaveMode(Src_t des);
void IIC_RT_EnterMasterMode(Src_t des);
HAL_StatusTypeDef IIC_RT_Send(Src_t des, uint16_t devAddr, uint8_t *data, uint16_t len);

#endif
