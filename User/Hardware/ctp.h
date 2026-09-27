#ifndef __CTP_H
#define __CTP_H	
//#include "sys.h"
#include "stm32f4xx.h"                  // Device header




//����ݴ��������ӵ�оƬ����(δ����IIC����) 
//IO操作
#define FT_RST(x)    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)  // FT6336复位引脚 PC3
#define FT_INT       HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0)      // FT6336中断引脚 PC0


//I2C��д����	
#define FT_CMD_WR 				0X70    	//д����
#define FT_CMD_RD 				0X71		//������
  
//FT6336 ���ּĴ������� 
#define FT_REG_NUM_FINGER       0x02		//����״̬�Ĵ���

#define FT_TP1_REG 				0X03	  //��һ�����������ݵ�ַ
#define FT_TP2_REG 				0X09		//�ڶ������������ݵ�ַ
#define FT_TP3_REG 				0X0F		//���������������ݵ�ַ
#define FT_TP4_REG 				0X15		//���ĸ����������ݵ�ַ
#define FT_TP5_REG 				0X1B		//��������������ݵ�ַ  
 


uint8_t FT6336_WR_Reg(uint16_t reg,uint8_t *buf,uint8_t len);
void FT6336_RD_Reg(uint16_t reg,uint8_t *buf,uint8_t len);
void FT6336_Init(void);
uint8_t FT6336_Scan(uint8_t mode);

#endif

















