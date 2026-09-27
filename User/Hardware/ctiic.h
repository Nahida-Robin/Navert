#ifndef __MYCT_IIC_H
#define __MYCT_IIC_H
//#include "sys.h"	    
#include "stm32f4xx.h"                  // Device header



//IO��������
#define CT_SDA_IN()   CT_SDA_SetInput()
#define CT_SDA_OUT()  CT_SDA_SetOutput()

/* 函数声明 */
void CT_SDA_SetInput(void);
void CT_SDA_SetOutput(void);


//IO操作
#define CT_IIC_SCL(x)    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, (x))
#define CT_IIC_SDA(x)    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, (x))
#define CT_READ_SDA   HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2)
 

//IIC���в�������
void CT_IIC_Init(void);                	//��ʼ��IIC��IO��				 
void CT_IIC_Start(void);				//����IIC��ʼ�ź�
void CT_IIC_Stop(void);	  				//����IICֹͣ�ź�
void CT_IIC_Send_Byte(uint8_t txd);			//IIC����һ���ֽ�
uint8_t CT_IIC_Read_Byte(unsigned char ack);	//IIC��ȡһ���ֽ�
uint8_t CT_IIC_Wait_Ack(void); 				//IIC�ȴ�ACK�ź�
void CT_IIC_Ack(void);					//IIC����ACK�ź�
void CT_IIC_NAck(void);					//IIC������ACK�ź�

#endif







