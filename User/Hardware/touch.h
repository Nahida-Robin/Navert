#ifndef __TOUCH_H__
#define __TOUCH_H__
//#include "sys.h"       
#include "CTP.h"	   
#include "stm32f4xx.h"                  // Device header




#define TP_PRES_DOWN 0x80  //����������	  
#define TP_CATH_PRES 0x40  //�а��������� 
#define CT_MAX_TOUCH  5    //������֧�ֵĵ���,�̶�Ϊ5��

//������������
typedef struct
{
	uint8_t (*init)(void);			//��ʼ��������������
	uint8_t (*scan)(uint8_t);				//ɨ�败����.0,��Ļɨ��;1,��������;	 
	void (*adjust)(void);		//������У׼ 
	uint16_t x[CT_MAX_TOUCH]; 		//��ǰ����
	uint16_t y[CT_MAX_TOUCH];		//�����������5������,����������x[0],y[0]����:�˴�ɨ��ʱ,����������,��
								//x[4],y[4]�洢��һ�ΰ���ʱ������. 
	uint8_t  sta;					//�ʵ�״̬ 
								//b7:����1/�ɿ�0; 
	                            //b6:0,û�а�������;1,�а�������. 
								//b5:����
								//b4~b0:���ݴ��������µĵ���(0,��ʾδ����,1��ʾ����)
/////////////////////������У׼����(����������ҪУ׼)//////////////////////								
	float xfac;					
	float yfac;
	short xoff;
	short yoff;	   
//�����Ĳ���,��������������������ȫ�ߵ�ʱ��Ҫ�õ�.
//b0:0,����(�ʺ�����ΪX����,����ΪY�����TP)
//   1,����(�ʺ�����ΪY����,����ΪX�����TP) 
//b1~6:����.
//b7:0,������
//   1,������ 
	uint8_t touchtype;
}_m_tp_dev;

extern _m_tp_dev tp_dev;	 	//������������touch.c���涨��



//霍尔传感器 软件SPI 引脚 (用HAL GPIO操作替换原来的PCout/PCin位带宏)
#define TPEN  HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0)    // PC0  INT
#define TSDO  HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3)    // PA3  MISO
#define TSDI(x)  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)  // PC2  MOSI
#define TCLK(x)  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)  // PC1  CLK
#define TCS(x)   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)  // PC3  CS
   
//����������
void TP_Write_Byte(uint8_t num);						//�����оƬд��һ������
uint16_t TP_Read_AD(uint8_t CMD);							//��ȡADת��ֵ
uint16_t TP_Read_XOY(uint8_t xy);							//���˲��������ȡ(X/Y)
uint8_t TP_Read_XY(uint16_t *x,uint16_t *y);					//˫�����ȡ(X+Y)
uint8_t TP_Read_XY2(uint16_t *x,uint16_t *y);					//����ǿ�˲���˫���������ȡ
void TP_Drow_Touch_Point(uint16_t x,uint16_t y,uint16_t color);//��һ������У׼��
void TP_Draw_Big_Point(uint16_t x,uint16_t y,uint16_t color);	//��һ�����
void TP_Save_Adjdata(void);						//����У׼����
uint8_t TP_Get_Adjdata(void);						//��ȡУ׼����
void TP_Adjust(void);							//������У׼
void TP_Adj_Info_Show(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t x3,uint16_t y3,uint16_t fac);//��ʾУ׼��Ϣ
//������/������ ���ú���
uint8_t TP_Scan(uint8_t tp);								//ɨ��
uint8_t TP_Init(void);								//��ʼ��
 
#endif

















