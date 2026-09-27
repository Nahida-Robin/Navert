#ifndef __TRANSFER_H
#define __TRANSFER_H

#include "App.h"
#include "RingBuf.h"

extern RingBuf_t USART_RingBuf;
extern RingBuf_t CAN_RingBuf;
extern RingBuf_t IIC_RingBuf;
extern RingBuf_t SPI_RingBuf;

void Transfer_Init(void);
void Transfer_Poll(void);           
void Transfer_Forward(Msg_t *msg);
void Transfer_SetRoute(Src_t src, Src_t des);
void Transfer_SetCANId(Src_t src, uint32_t can_id);
void Transfer_SetCANIdMode(Src_t src, uint32_t ide);
void Transfer_SetCANMask(Src_t src, uint32_t mask);
void Transfer_SetIICAddr(Src_t src, uint16_t addr);
uint32_t Transfer_GetCANId(Src_t src);
uint32_t Transfer_GetCANIdMode(Src_t src);
uint32_t Transfer_GetCANMask(Src_t src);
uint16_t Transfer_GetIICAddr(Src_t src);
void Transfer_TxComplete(Src_t des);

#endif
