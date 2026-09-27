/** 
 * @file RingBuf.c
 * @brief 环形缓冲区实现
 * @author Nahida
 * @date 2026.9.10
 */

#include "RingBuf.h"

/**
  *@brief 环形缓冲区初始化
  *@param rb 环形缓冲区指针
  *@retval NULL
  */
void RingBuf_Init(RingBuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/**
  *@brief 环形缓冲区写入
  *@param rb 环形缓冲区指针
  *@param msg 待写入消息
  *@retval 0 写入失败 即缓冲区满
  *@retval 1 写入成功
  */
int RingBuf_Write(RingBuf_t *rb, const Msg_t *msg)
{
    uint8_t next = (uint8_t)((rb->head + 1) % RINGBUF_SIZE);//留一个空位置区分空和满
    //如（31+1）%32=0=tail，说明前面没读不能覆写，满了
    if (next == rb->tail)
        return 0;
    rb->buffer[rb->head] = *msg;
    rb->head = next;
    return 1;
}

/**
  *@brief 读环形缓冲区
  *@param rb 环形缓冲区指针
  *@param msg 读出信息
  *@retval 0 读出失败 即缓冲区空
  *@retval 1 读出成功
  */
int RingBuf_Read(RingBuf_t *rb, Msg_t *msg)
{
    if (rb->head == rb->tail)
        return 0;
    *msg = rb->buffer[rb->tail];
    rb->tail = (uint8_t)((rb->tail + 1) % RINGBUF_SIZE);
    return 1;
}
