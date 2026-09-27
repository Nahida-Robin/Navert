#ifndef __CAN_RT_H
#define __CAN_RT_H

#include "App.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

#define CAN_DATA_MAX    8

//CAN配置结构体
typedef struct {
    uint32_t Prescaler;
    uint32_t Mode;
    uint32_t SyncJumpWidth;
    uint32_t TimeSeg1;
    uint32_t TimeSeg2;
    FunctionalState AutoBusOff;
    FunctionalState AutoRetransmission;
} CAN_Config_t;

void CAN_RT_Init(void);
void CAN_RT_Config(Src_t des, CAN_Config_t *cfg);
HAL_StatusTypeDef CAN_RT_Send(Src_t des, uint32_t id, uint32_t ide, uint8_t *data, uint16_t len);

#endif
