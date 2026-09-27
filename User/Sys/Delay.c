/** 
 * @file Delay.c
 * @brief DWT外设延时
 * @author Nahida
 * @date 2026.5.24
 */

#include "stm32f4xx.h"                  // Device header

/**
  *@brief DWT外设初始化
  *@param NULL
  *@retval NULL
  */
void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // DWT外设使能
    DWT->CYCCNT = 0;                                 // 初始化计数器
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // CYCCNT使能
}

/**
  *@brief us延时
  *@param us us延时数
  *@retval NULL
  */
void Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);

    while ((DWT->CYCCNT - start) < ticks);
}

/**
  *@brief ms延时
  *@param ms ms延时数
  *@retval NULL
  */
void Delay_ms(uint32_t ms)
{
    while (ms--)
    {
        Delay_us(1000);
    }
}
