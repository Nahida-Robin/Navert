#ifndef __APP_H
#define __APP_H

#include "stm32f4xx.h"                  // Device header
#include "main.h"

//端口名枚举
typedef enum {
	mCAN1 = 0,
	mCAN2,
	mUSART1,
	mUSART2,
	mIIC1,
	mIIC2,
	mSPI2,
	mSPI3
} Src_t;

//消息结构体
typedef struct {
	Src_t src;
	uint16_t len;
	uint8_t data[256];
} Msg_t;

#endif
