#ifndef __RINGBUF_H
#define __RINGBUF_H

#include "App.h"

#define RINGBUF_SIZE    32

//RingBuf结构体
typedef struct {
    Msg_t       buffer[RINGBUF_SIZE];
    volatile uint8_t head;//写缓冲区序号 中断写 主循环读
    volatile uint8_t tail;//读缓冲区序号 主循环写 中断读
} RingBuf_t;

void RingBuf_Init(RingBuf_t *rb);
int  RingBuf_Write(RingBuf_t *rb, const Msg_t *msg);
int  RingBuf_Read(RingBuf_t *rb, Msg_t *msg);

#endif
