#ifndef __LCD_H
#define __LCD_H		

	 
/////////////////////////////////////�û�������///////////////////////////////////	 
//֧�ֺ��������ٶ����л�
#define USE_LCM_DIR  	  0   	//����Һ����˳ʱ����ת���� 	0-0����ת��1-180����ת��2-270����ת��3-90����ת
#define USE_TP_TYPE   	CTP 	//���崥������  CTP = ����ģʽ , RTP = ����ģʽ

#include "stm32f4xx_hal.h" 

#define CTP 0X80		//����ģʽ
#define RTP 0X00		//����ģʽ


//LCD��Ҫ������
typedef struct  
{										    
	uint16_t width;			//LCD ����
	uint16_t height;			//LCD �߶�
	uint16_t id;				//LCD ID
	uint8_t  dir;			//���������������ƣ�0��������1��������	
	uint16_t	wramcmd;		//��ʼдgramָ��
	uint16_t setxcmd;		//����x����ָ��
	uint16_t  setycmd;		//����y����ָ��	 
}_lcd_dev; 	  

//LCD����
extern _lcd_dev lcddev;	//����LCD��Ҫ����
//LCD�Ļ�����ɫ�ͱ���ɫ	   
extern uint16_t  POINT_COLOR;//Ĭ�Ϻ�ɫ    
extern uint16_t  BACK_COLOR; //������ɫ.Ĭ��Ϊ��ɫ

////////////////////////////////////////////////////////////////////
//-----------------LCD�˿ڶ���----------------// 

	
#define SPI_CS(x)  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET);			//Ƭѡ�˿�							  	PA6
#define SPI_RST(x) HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET);			//��λ  	  											PA5
#define SPI_DC(x) HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET);			//����/����      								PB1  
#define	LCD_LED(x) HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET); 			//���� ����                     PB11

#define LCD_Delay_us(x)  { uint32_t cnt = (x) * 42; while(cnt--); }  // ??168MHz? �1us	
	 

//������ɫ
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //��ɫ
#define BRRED 			 0XFC07 //�غ�ɫ
#define GRAY  			 0X8430 //��ɫ
//GUI��ɫ

#define DARKBLUE      	 0X01CF	//����ɫ
#define LIGHTBLUE      	 0X7D7C	//ǳ��ɫ  
#define GRAYBLUE       	 0X5458 //����ɫ
//������ɫΪPANEL����ɫ 
 
#define LIGHTGREEN     	 0X841F //ǳ��ɫ 
#define LGRAY 					 0XC618 //ǳ��ɫ(PANNEL),���屳��ɫ

#define LGRAYBLUE        0XA651 //ǳ����ɫ(�м����ɫ)
#define LBBLUE           0X2B12 //ǳ����ɫ(ѡ����Ŀ�ķ�ɫ)
	    															  
	    															  
void LCD_Init(void);													   	//��ʼ��
void LCD_DisplayOn(void);													//����ʾ
void LCD_DisplayOff(void);													//����ʾ
void LCD_Clear(uint16_t Color);	 												//����
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos);										//���ù��
void LCD_DrawPoint(uint16_t x,uint16_t y);											//����
void LCD_Fast_DrawPoint(uint16_t x,uint16_t y,uint16_t color);								//���ٻ���
void LCD_Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r);						 			//��Բ
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);							//����
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);		   				//������
void LCD_Fill(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t color);		   				//��䵥ɫ
void LCD_Color_Fill(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t *color);				//���ָ����ɫ
void LCD_ShowChar(uint16_t x,uint16_t y,uint8_t num,uint8_t size,uint8_t mode);						//��ʾһ���ַ�
void LCD_ShowNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t size);  						//��ʾһ������
void LCD_ShowxNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t size,uint8_t mode);				//��ʾ ����
void LCD_ShowString(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t size,uint8_t *p);		//��ʾһ���ַ���,12/16����

void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue);
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(uint16_t RGB_Code);
void Load_Drow_Dialog(void);
void LCD_Set_Window(uint16_t sx,uint16_t sy,uint16_t width,uint16_t height);	//���ô���				
void Show_Str(uint16_t x, uint16_t y,uint8_t *str,uint8_t size,uint8_t mode);//��ʾ����
void Gui_Drawbmp16(uint16_t x,uint16_t y,const unsigned char *p); //��ʾ40*40 ͼƬ
void Gui_StrCenter(uint16_t x, uint16_t y,uint8_t *str,uint8_t size,uint8_t mode);//������ʾ
void LCD_Display_Dir(uint8_t dir); //����LCD��ʾ����
void lcd_draw_bline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,uint8_t size,uint16_t color);//��һ������
void gui_fill_circle(uint16_t x0,uint16_t y0,uint16_t r,uint16_t color);//��ʵ��Բ
void gui_draw_hline(uint16_t x0,uint16_t y0,uint16_t len,uint16_t color);//��ˮƽ�� ���ݴ�����ר�в���
uint16_t LCD_ReadPoint(uint16_t x,uint16_t y);//��ȡָ��������ɫ

// LVGL 非阻塞 DMA 发送 (disp_drv 以 void* 传入，避免头文件依赖 lvgl.h)
void LCD_DMA_Send_IT(uint8_t *data, uint32_t size, void *disp_drv);



#endif













