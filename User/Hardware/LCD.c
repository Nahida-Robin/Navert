#include "lcd.h"
#include "stdlib.h"
#include "font.h"
//#include "delay.h"
#include "string.h"
#include "stm32f4xx.h"                  // Device header
#include "lvgl.h"                       // LVGL disp_flush_ready
#include "Transfer.h"                    // Transfer_TxComplete for SPI2/SPI3 forwarding


//LCD的基本颜色和背景色
uint16_t POINT_COLOR=0x0000;	//刷屏颜色
uint16_t BACK_COLOR=0xFFFF;  //背景色

extern SPI_HandleTypeDef hspi1;

//=====================================
// DMA 搬运支持 (SPI1_TX: DMA2 Stream3)
//=====================================

// DMA 传输完成标志 (在中断中置位)
static volatile uint8_t lcd_dma_busy = 0;

// LVGL 非阻塞 DMA 控制
static volatile uint8_t lvgl_dma_active = 0;    // 当前 DMA 是否为 LVGL 发起
static lv_disp_drv_t *lvgl_disp_drv = NULL;     // LVGL display driver 指针

// 行缓冲区: 最大 320 像素 × 2 字节 = 640 字节
// 用于字节序转换: STM32小端 → SPI MSB-first
#define LCD_DMA_BUF_BYTES  (320 * 2)
static uint8_t lcd_dma_buf[LCD_DMA_BUF_BYTES];

/**
  * @brief SPI1 DMA 发送完成回调 (覆写 HAL 弱定义)
  *
  * 先等待 SPI 总线空闲（确保最后一位已移出），再处理：
  * - LVGL 非阻塞路径：由 ISR 拉高 CS 并调用 lv_disp_flush_ready()
  * - 阻塞 DMA 路径：只清忙标志，CS 由调用者管理
  */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi->Instance == SPI1)
	{
		/* !!! 关键必须先等 SPI 移完最后一位，再操作 CS !!! */
		while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_BSY) != RESET);

		if (lvgl_dma_active)
		{
			lvgl_dma_active = 0;
			SPI_CS(1);                              // 结束 SPI 事务
			lcd_dma_busy = 0;
			if (lvgl_disp_drv)
			{
				lv_disp_flush_ready(lvgl_disp_drv);
			}
		}
		else
		{
			lcd_dma_busy = 0;                       // 阻塞路径：只清标志，CS 由调用者管理
		}
	}
	else if (hspi->Instance == SPI2)
	{
		Transfer_TxComplete(mSPI2);
	}
	else if (hspi->Instance == SPI3)
	{
		Transfer_TxComplete(mSPI3);
	}
}

/**
  * @brief 等待 DMA 传输完成 (轮询标志)
  */
static void LCD_DMA_Wait(void)
{
	while (lcd_dma_busy);
}

/**
  * @brief 通过 SPI DMA 发送数据 (调用前需 CS=0, DC 已设置)
  * @param data 数据指针
  * @param size 字节数
  */
static void LCD_DMA_Send(uint8_t *data, uint32_t size)
{
	lcd_dma_busy = 1;
	HAL_SPI_Transmit_DMA(&hspi1, data, size);
	LCD_DMA_Wait();
}

/**
  * @brief 非阻塞 SPI DMA 发送 (LVGL disp_flush 使用)
  * 立即返回，DMA 完成后由 HAL_SPI_TxCpltCallback 自动拉高 CS
  * 并调用 lv_disp_flush_ready()。
  * @param data    数据指针 (LVGL 帧缓冲)
  * @param size    字节数
  * @param disp_drv 当前 LVGL display driver 指针
  */
void LCD_DMA_Send_IT(uint8_t *data, uint32_t size, void *disp_drv)
{
	lvgl_disp_drv = (lv_disp_drv_t *)disp_drv;
	lvgl_dma_active = 1;
	HAL_SPI_Transmit_DMA(&hspi1, data, size);
	/* 立即返回，不做等待 */
}

/**
  * @brief 用 16 位颜色值预填充行缓冲区 (MSB-first 字节序)
  * @param color RGB565 颜色值
  * @param count 像素数
  */
static void LCD_DMA_FillBuf(uint16_t color, uint16_t count)
{
	uint8_t hi = (uint8_t)(color >> 8);
	uint8_t lo = (uint8_t)(color & 0xFF);
	uint16_t i;
	for (i = 0; i < count; i++)
	{
		lcd_dma_buf[i * 2]     = hi;	// 高字节先发
		lcd_dma_buf[i * 2 + 1] = lo;	// 低字节后发
	}
}

//定义LCD的重要参数
//默认为横屏
_lcd_dev lcddev;

//写寄存器函数
//data:寄存器值
void LCD_WR_REG(uint16_t data)
{
		uint8_t i;
		uint8_t cmd = (uint8_t)data;
		SPI_CS(0);
		SPI_DC(0);//RS=0 命令
		HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
//		for(i=0; i<8; i++)
//		{
//			if (data & 0x80)
//			{SPI_SDI(1);}
//			else
//			{SPI_SDI(0);}

//			data <<= 1;
//			SPI_SCK(0);
//			LCD_Delay_us(1);
//			SPI_SCK(1);
//			LCD_Delay_us(1);
//		}
		SPI_CS(1);
}
//写数据函数
//data:寄存器值
void LCD_WR_DATA(uint16_t data)
{
		uint8_t i;
		uint8_t dat = (uint8_t)data;
		SPI_CS(0);
		SPI_DC(1);//RS=1 数据

		HAL_SPI_Transmit(&hspi1, &dat, 1, 10);
//		for(i=0; i<8; i++)
//		{
//			if (data & 0x80)
//			{SPI_SDI(1);}
//			else
//			{SPI_SDI(0);}

//			data <<= 1;
//			SPI_SCK(0);
//			SPI_SCK(1);
//		}
		SPI_CS(1);
}

//写寄存器
//LCD_Reg:寄存器地址
//LCD_RegValue:要写入的值
void LCD_WriteReg(uint16_t LCD_Reg,uint16_t LCD_RegValue)
{
	LCD_WR_REG(LCD_Reg);
	LCD_WriteRAM(LCD_RegValue);
}

//开始写GRAM
void LCD_WriteRAM_Prepare(void)
{
	LCD_WR_REG(lcddev.wramcmd);
}
//LCD写GRAM
//RGB_Code:颜色值
void LCD_WriteRAM(uint16_t RGB_Code)
{
//写十六位GRAM
  uint8_t i;
	SPI_CS(0);
	SPI_DC(1);

	uint8_t buf[2];
	buf[0] = (uint8_t)(RGB_Code >> 8);
	buf[1] = (uint8_t)(RGB_Code & 0xFF);
	
	HAL_SPI_Transmit(&hspi1, buf, 2, 10);
//	for(i=0; i<16; i++)
//	{
//		if (RGB_Code & 0x8000)
//		{SPI_SDI(1);}
//		else
//		{SPI_SDI(0);}

//		RGB_Code <<= 1;
//		SPI_SCK(0);
//		SPI_SCK(1);
//	}
	SPI_CS(1);
}

//当mdk -O1时优化时需要调用
//延时i
void opt_delay(uint8_t i)
{
	while(i--);
}

//LCD开启显示
void LCD_DisplayOn(void)
{
	LCD_WR_REG(0X29);	//开启显示
}
//LCD关闭显示
void LCD_DisplayOff(void)
{
	LCD_WR_REG(0X28);	//关闭显示
}
//设置光标位置
//Xpos:列坐标
//Ypos:行坐标
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
{
		LCD_WR_REG(lcddev.setxcmd);
		LCD_WR_DATA(Xpos>>8);LCD_WR_DATA(Xpos&0XFF);
		LCD_WR_REG(lcddev.setycmd);
		LCD_WR_DATA(Ypos>>8);LCD_WR_DATA(Ypos&0XFF);
}

//画点
//x,y:坐标
//POINT_COLOR:此点的颜色
void LCD_DrawPoint(uint16_t x,uint16_t y)
{
	LCD_SetCursor(x,y);		//设置光标位置
	LCD_WriteRAM_Prepare();	//开始写入GRAM
	LCD_WriteRAM(POINT_COLOR);
}
//快速画点
//x,y:坐标
//color:颜色
void LCD_Fast_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{
		//设置光标位置
		LCD_SetCursor(x,y);
		//写入颜色
		LCD_WriteReg(lcddev.wramcmd,color);
}


//dir:方向选择 	0-0度旋转；1-180度旋转；2-270度旋转；3-90度旋转
void LCD_Display_Dir(uint8_t dir)
{
	if(dir==0||dir==1)			//竖屏
	{
			lcddev.dir=0;	//竖屏
			lcddev.width=240;
			lcddev.height=320;

			lcddev.wramcmd=0X2C;
			lcddev.setxcmd=0X2A;
			lcddev.setycmd=0X2B;

		if(dir==0)        //0-0度旋转
		{
			LCD_WR_REG(0x36);
			LCD_WR_DATA((0<<3)|(0<<7)|(0<<6)|(0<<5));
		}else							//1-180度旋转
		{
			LCD_WR_REG(0x36);
			LCD_WR_DATA((0<<3)|(1<<7)|(1<<6)|(0<<5));
		}

	}else if(dir==2||dir==3)
	{

			lcddev.dir=1;	//横屏
			lcddev.width=320;
			lcddev.height=240;

			lcddev.wramcmd=0X2C;
			lcddev.setxcmd=0X2A;
			lcddev.setycmd=0X2B;

				if(dir==2)				//2-270度旋转
				{
					LCD_WR_REG(0x36);
					LCD_WR_DATA((0<<3)|(1<<7)|(0<<6)|(1<<5));

				}else							//3-90度旋转
				{
					LCD_WR_REG(0x36);
					LCD_WR_DATA((0<<3)|(0<<7)|(1<<6)|(1<<5));
				}
	}


		//设置显示窗口
		LCD_WR_REG(lcddev.setxcmd);
		LCD_WR_DATA(0);LCD_WR_DATA(0);
		LCD_WR_DATA((lcddev.width-1)>>8);LCD_WR_DATA((lcddev.width-1)&0XFF);
		LCD_WR_REG(lcddev.setycmd);
		LCD_WR_DATA(0);LCD_WR_DATA(0);
		LCD_WR_DATA((lcddev.height-1)>>8);LCD_WR_DATA((lcddev.height-1)&0XFF);


}
//设置窗口,并自动将用户坐标定位到窗口左上角(sx,sy).
//sx,sy:窗口起始坐标(左上角)
//width,height:窗口宽度和高度,必须大于0!!
//窗口大小:width*height.
void LCD_Set_Window(uint16_t sx,uint16_t sy,uint16_t width,uint16_t height)
{
	uint16_t twidth,theight;
	twidth=sx+width-1;
	theight=sy+height-1;

		LCD_WR_REG(lcddev.setxcmd);
		LCD_WR_DATA(sx>>8);
		LCD_WR_DATA(sx&0XFF);
		LCD_WR_DATA(twidth>>8);
		LCD_WR_DATA(twidth&0XFF);
		LCD_WR_REG(lcddev.setycmd);
		LCD_WR_DATA(sy>>8);
		LCD_WR_DATA(sy&0XFF);
		LCD_WR_DATA(theight>>8);
		LCD_WR_DATA(theight&0XFF);

}
//初始化lcd
void LCD_Init(void)
{

//	RCC->APB2ENR|=1<<2;//使能PORTA时钟
// 	RCC->APB2ENR|=1<<3;//使能PORTB时钟
// 	RCC->APB2ENR|=1<<4;//使能PORTC时钟

//	RCC->APB2ENR|=1<<0;    //辅助时钟
////	JTAG_Set(SWD_ENABLE);  //设置SWD
//
//	GPIOA->CRL&=0XFFFFF0FF;
//	GPIOA->CRL|=0X00000300;//LED(背光开关) = PA2
//	GPIOA->CRH&=0X0FF00FFF;
//	GPIOA->CRH|=0X30038000;//SPI_SDO = PA11  CS = PA12   DC = PA15
//	GPIOA->ODR|=0X9804;    //IO设置
//
//	GPIOC->CRH&=0X000FFFFF;
//	GPIOC->CRH|=0X33300000;//SDI = PC13  SCK = PC14  RST = PC15
//	GPIOC->ODR|=0XE000;    //IO设置



	SPI_RST(1);
	HAL_Delay(10);
	SPI_RST(0);
	HAL_Delay(20);
	SPI_RST(1);
	HAL_Delay(150);

	LCD_WR_REG(0x01);    // Software Reset
	HAL_Delay(150);

//************* Start Initial Sequence **********//
LCD_WR_REG(0x11);     // Sleep Out
HAL_Delay(120);                //delay_ms 120ms
LCD_WR_REG(0x36);
LCD_WR_DATA(0x00);
LCD_WR_REG(0x3A);
LCD_WR_DATA(0x55);
LCD_WR_REG(0xB2);
LCD_WR_DATA(0x0C);
LCD_WR_DATA(0x0C);
LCD_WR_DATA(0x00);
LCD_WR_DATA(0x33);
LCD_WR_DATA(0x33);
LCD_WR_REG(0xB7);
LCD_WR_DATA(0x75);
LCD_WR_REG(0xBB);
LCD_WR_DATA(0x15);
LCD_WR_REG(0xC0);
LCD_WR_DATA(0x2C);
LCD_WR_REG(0xC2);
LCD_WR_DATA(0x01);
LCD_WR_REG(0xC3);
LCD_WR_DATA(0x13);
LCD_WR_REG(0xC4);
LCD_WR_DATA(0x23);
LCD_WR_REG(0xC6);
LCD_WR_DATA(0x0F);
LCD_WR_REG(0xD0);
LCD_WR_DATA(0xA4);
LCD_WR_DATA(0xA1);
LCD_WR_REG(0xD6);
LCD_WR_DATA(0xA1);
LCD_WR_REG(0x21);
LCD_WR_REG(0xE0);
LCD_WR_DATA(0xD0);
LCD_WR_DATA(0x08);
LCD_WR_DATA(0x10);
LCD_WR_DATA(0x0D);
LCD_WR_DATA(0x0C);
LCD_WR_DATA(0x07);
LCD_WR_DATA(0x37);
LCD_WR_DATA(0x53);
LCD_WR_DATA(0x4C);
LCD_WR_DATA(0x39);
LCD_WR_DATA(0x15);
LCD_WR_DATA(0x15);
LCD_WR_DATA(0x2A);
LCD_WR_DATA(0x2D);
LCD_WR_REG(0xE1);
LCD_WR_DATA(0xD0);
LCD_WR_DATA(0x0D);
LCD_WR_DATA(0x12);
LCD_WR_DATA(0x08);
LCD_WR_DATA(0x08);
LCD_WR_DATA(0x15);
LCD_WR_DATA(0x34);
LCD_WR_DATA(0x34);
LCD_WR_DATA(0x4A);
LCD_WR_DATA(0x36);
LCD_WR_DATA(0x12);
LCD_WR_DATA(0x13);
LCD_WR_DATA(0x2B);
LCD_WR_DATA(0x2F);
LCD_WR_REG(0x29); // Display on

}



//获取某点的颜色值
//x,y:坐标
//返回值:此点的颜色
//uint16_t LCD_ReadPoint(uint16_t x,uint16_t y)
//{
//	uint8_t i,r,g,b,reg=0x2e;
// 	uint16_t color;
//	if(x>=lcddev.width||y>=lcddev.height)return 0;	//超过了范围,直接返回
//	LCD_SetCursor(x,y);
//	SPI_CS(0);
//	SPI_DC(0);

//		for(i=0; i<8; i++)
//		{
//			if (reg & 0x80)
//			{SPI_SDI(1);}
//			else
//			{SPI_SDI(0);}

//			reg <<= 1;
//			SPI_SCK(0);
//			SPI_SCK(1);
//		}



//		for(i=0; i<8; i++)							//第一次空读 后三次分别为R G B
//		{
//			SPI_SCK(0);
//			SPI_SCK(1);
//		}


//		for(i=0; i<8; i++)
//		{
//			SPI_SCK(0);		r=r << 1 | SPI_SDO;
//			SPI_SCK(1);
//		}



//		for(i=0; i<8; i++)
//		{
//			SPI_SCK(0);		g=g << 1 | SPI_SDO;
//			SPI_SCK(1);
//		}


//		for(i=0; i<8; i++)
//		{
//			SPI_SCK(0);		b=b << 1 | SPI_SDO;
//			SPI_SCK(1);
//		}

//		color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);


//		SPI_CS(1);


//return color;
//}
uint16_t LCD_ReadPoint(uint16_t x, uint16_t y)
{
    uint8_t reg = 0x2E;  // 读GRAM命令
    uint8_t dummy, r, g, b;
    uint16_t color;
    
    if(x >= lcddev.width || y >= lcddev.height) return 0;
    
    LCD_SetCursor(x, y);
    
    // 发送读命令 0x2E
    SPI_CS(0);
    SPI_DC(0);
    HAL_SPI_Transmit(&hspi1, &reg, 1, 10);
    
    // 空读一次（dummy）
    SPI_DC(1);
    HAL_SPI_Receive(&hspi1, &dummy, 1, 10);
    
    // 读R
    HAL_SPI_Receive(&hspi1, &r, 1, 10);
    // 读G
    HAL_SPI_Receive(&hspi1, &g, 1, 10);
    // 读B
    HAL_SPI_Receive(&hspi1, &b, 1, 10);
    
    SPI_CS(1);
    
    color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return color;
}


//清屏函数 (DMA 加速)
//color:要清屏的颜色
void LCD_Clear(uint16_t color)
{
	uint32_t total = (uint32_t)lcddev.width * lcddev.height;
	uint32_t remain = total;
	uint32_t batch;

	LCD_SetCursor(0x00, 0x0000);		//设置光标位置
	LCD_WriteRAM_Prepare();     		//开始写入GRAM

	// 用颜色预填一行缓冲区
	LCD_DMA_FillBuf(color, lcddev.width);

	SPI_CS(0);							// CS 低: 一次片选保持整个数据帧
	SPI_DC(1);							// DC 高: 数据模式

	while (remain)
	{
		batch = (remain > lcddev.width) ? lcddev.width : remain;
		LCD_DMA_Send(lcd_dma_buf, batch * 2);
		remain -= batch;
	}

	SPI_CS(1);							// CS 高: 释放片选
}
//在指定区域内填充指定颜色 (DMA 加速)
//区域大小:(xend-xsta+1)*(yend-ysta+1)
//xsta
//color:要填充的颜色
void LCD_Fill(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t color)
{
	uint16_t width, height;
	uint32_t total, remain;
	uint32_t batch;

	if((lcddev.id==0X6804)&&(lcddev.dir==1))	//6804竖屏时特殊处理
	{
		uint16_t temp;

		temp=sx;
		sx=sy;
		sy=lcddev.width-ex-1;
		ex=ey;
		ey=lcddev.width-temp-1;
		lcddev.dir=0;
		lcddev.setxcmd=0X2A;
		lcddev.setycmd=0X2B;
		LCD_Fill(sx,sy,ex,ey,color);
		lcddev.dir=1;
		lcddev.setxcmd=0X2B;
		lcddev.setycmd=0X2A;
	}
	else
	{
		width  = ex - sx + 1;
		height = ey - sy + 1;
		total  = (uint32_t)width * height;
		remain = total;

		// 通过 Set_Window 一次性设定区域, 无需逐行设光标
		LCD_Set_Window(sx, sy, width, height);
		LCD_WriteRAM_Prepare();

		// 预填行缓冲区 (一行宽度)
		LCD_DMA_FillBuf(color, width);

		SPI_CS(0);
		SPI_DC(1);

		while (remain)
		{
			batch = (remain > width) ? width : remain;
			LCD_DMA_Send(lcd_dma_buf, batch * 2);
			remain -= batch;
		}

		SPI_CS(1);
	}
}
//在指定区域内填充指定颜色块 (DMA 加速)
//(sx,sy),(ex,ey):矩形的对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)
//color:要填充的颜色 (uint16_t 数组, 小端原生字节序)
void LCD_Color_Fill(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t *color)
{
	uint16_t width, height;
	uint32_t total, remain;
	uint32_t batch;

	width  = ex - sx + 1;				//得到区域的宽度
	height = ey - sy + 1;				//高度
	total  = (uint32_t)width * height;
	remain = total;

	// 设定窗口, 一次性传送所有像素
	LCD_Set_Window(sx, sy, width, height);
	LCD_WriteRAM_Prepare();

	SPI_CS(0);
	SPI_DC(1);

	while (remain)
	{
		batch = (remain > (LCD_DMA_BUF_BYTES / 2)) ? (LCD_DMA_BUF_BYTES / 2) : remain;

		// 字节序转换: 小端 uint16_t → MSB-first (高字节在前)
		for (uint32_t ic = 0; ic < batch; ic++)
		{
			lcd_dma_buf[ic * 2]     = (uint8_t)(color[ic] >> 8);
			lcd_dma_buf[ic * 2 + 1] = (uint8_t)(color[ic] & 0xFF);
		}

		LCD_DMA_Send(lcd_dma_buf, batch * 2);
		color += batch;
		remain -= batch;
	}

	SPI_CS(1);
}
//画线
//x1,y1:起点坐标
//x2,y2:终点坐标
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint16_t t;
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //计算坐标差
	delta_y=y2-y1;
	uRow=x1;
	uCol=y1;
	if(delta_x>0)incx=1; //设置单步方向
	else if(delta_x==0)incx=0;//垂直线
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if(delta_y==0)incy=0;//水平线
	else{incy=-1;delta_y=-delta_y;}
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴
	else distance=delta_y;
	for(t=0;t<=distance+1;t++ )//画线输出
	{
		LCD_DrawPoint(uRow,uCol);//画点
		xerr+=delta_x ;
		yerr+=delta_y ;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}
//画矩形
//(x1,y1),(x2,y2):矩形的对角坐标
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	LCD_DrawLine(x1,y1,x2,y1);
	LCD_DrawLine(x1,y1,x1,y2);
	LCD_DrawLine(x1,y2,x2,y2);
	LCD_DrawLine(x2,y1,x2,y2);
}
//在指定位置画一个指定大小的圆
//(x,y):中心点
//r    :半径
void LCD_Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r)
{
	int a,b;
	int di;
	a=0;b=r;
	di=3-(r<<1);             //判断下个点位置的标志
	while(a<=b)
	{
		LCD_DrawPoint(x0+a,y0-b);             //5
	 	LCD_DrawPoint(x0+b,y0-a);             //0
		LCD_DrawPoint(x0+b,y0+a);             //4
		LCD_DrawPoint(x0+a,y0+b);             //6
		LCD_DrawPoint(x0-a,y0+b);             //1
	 	LCD_DrawPoint(x0-b,y0+a);
		LCD_DrawPoint(x0-a,y0-b);             //2
  	LCD_DrawPoint(x0-b,y0-a);             //7
		a++;
		//使用Bresenham算法画圆
		if(di<0)di +=4*a+6;
		else
		{
			di+=10+4*(a-b);
			b--;
		}
	}
}
//在指定位置显示一个字符
//x,y:起始坐标
//num:要显示的字符:" "--->"~"
//size:字体大小 12/16/24
//mode:叠加方式(1)还是非叠加方式(0)
void LCD_ShowChar(uint16_t x,uint16_t y,uint8_t num,uint8_t size,uint8_t mode)
{
    uint8_t temp,t1,t;
	uint16_t y0=y;
	uint8_t csize=(size/8+((size%8)?1:0))*(size/2);		//得到字体一个字符对应点阵集所占的字节数
 	num=num-' ';//得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库）
	for(t=0;t<csize;t++)
	{
		if(size==12)temp=asc2_1206[num][t]; 	 	//调用1206字体
		else if(size==16)temp=asc2_1608[num][t];	//调用1608字体
		else if(size==24)temp=asc2_2412[num][t];	//调用2412字体
		else return;								//没有的字库
		for(t1=0;t1<8;t1++)
		{
			if(temp&0x80)LCD_Fast_DrawPoint(x,y,POINT_COLOR);
			else if(mode==0)LCD_Fast_DrawPoint(x,y,BACK_COLOR);
			temp<<=1;
			y++;
			if(y>=lcddev.height)return;		//超区域了
			if((y-y0)==size)
			{
				y=y0;
				x++;
				if(x>=lcddev.width)return;	//超区域了
				break;
			}
		}
	}
}
//m^n函数
//返回值:m^n次方.
uint32_t LCD_Pow(uint8_t m,uint8_t n)
{
	uint32_t result=1;
	while(n--)result*=m;
	return result;
}
//显示数字,高位为0,不显示
//x,y :起点坐标
//len :数字的位数
//size:字体大小
//color:颜色
//num:数值(0~4294967295);
void LCD_ShowNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t size)
{
	uint8_t t,temp;
	uint8_t enshow=0;
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+(size/2)*t,y,' ',size,0);
				continue;
			}else enshow=1;

		}
	 	LCD_ShowChar(x+(size/2)*t,y,temp+'0',size,0);
	}
}
//显示数字,高位为0,还是显示
//x,y:起点坐标
//num:数值(0~999999999);
//len:长度(即要显示的位数)
//size:字体大小
//mode:
//[7]:0,不填充;1,填充0.
//[6:1]:保留
//[0]:0,非叠加显示;1,叠加显示.
void LCD_ShowxNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t size,uint8_t mode)
{
	uint8_t t,temp;
	uint8_t enshow=0;
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				if(mode&0X80)LCD_ShowChar(x+(size/2)*t,y,'0',size,mode&0X01);
				else LCD_ShowChar(x+(size/2)*t,y,' ',size,mode&0X01);
 				continue;
			}else enshow=1;

		}
	 	LCD_ShowChar(x+(size/2)*t,y,temp+'0',size,mode&0X01);
	}
}
//显示字符串
//x,y:起点坐标
//width,height:区域大小
//size:字体大小
//*p:字符串起始地址
void LCD_ShowString(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t size,uint8_t *p)
{
	uint8_t x0=x;
	width+=x;
	height+=y;
    while((*p<='~')&&(*p>=' '))//判断是不是非法字符!
    {
        if(x>=width){x=x0;y+=size;}
        if(y>=height)break;//退出
        LCD_ShowChar(x,y,*p,size,0);
        x+=size/2;
        p++;
    }
}


//显示 16*16
void GUI_DrawFont16(uint16_t x, uint16_t y,uint8_t *s,uint8_t mode)
{
	uint8_t i,j;
	uint16_t k;
	uint16_t HZnum;
	uint16_t x0=x;
	HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);	//自动统计汉字数目


	for (k=0;k<HZnum;k++)
	{
	  if ((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)))
	  { 	LCD_Set_Window(x,y,16,16);
				LCD_WriteRAM_Prepare();
		    for(i=0;i<16*2;i++)
		    {
				for(j=0;j<8;j++)
		    	{
					if(!mode) //非叠加方式
					{
						if(tfont16[k].Msk[i]&(0x80>>j))	LCD_WriteRAM(POINT_COLOR);
						else LCD_WriteRAM(BACK_COLOR);
					}
					else
					{
						//POINT_COLOR=fc;
						if(tfont16[k].Msk[i]&(0x80>>j))	LCD_DrawPoint(x,y);//画一个点
						x++;
						if((x-x0)==16)
						{
							x=x0;
							y++;
							break;
						}
					}

				}

			}


		}
		continue;  //已经找到对应的字库则退出，防止因重复取模影响速度
	}

	LCD_Set_Window(0,0,lcddev.width,lcddev.height);//恢复窗口为全屏
}

//显示 24*24
void GUI_DrawFont24(uint16_t x, uint16_t y, uint8_t *s,uint8_t mode)
{
	uint8_t i,j;
	uint16_t k;
	uint16_t HZnum;
	uint16_t x0=x;
	HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);	//自动统计汉字数目

			for (k=0;k<HZnum;k++)
			{
			  if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
			  { 	LCD_Set_Window(x,y,24,24);
						LCD_WriteRAM_Prepare();
				    for(i=0;i<24*3;i++)
				    {
							for(j=0;j<8;j++)
							{
								if(!mode) //非叠加方式
								{
									if(tfont24[k].Msk[i]&(0x80>>j))	LCD_WriteRAM(POINT_COLOR);
									else LCD_WriteRAM(BACK_COLOR);
								}
							else
							{
								//POINT_COLOR=fc;
								if(tfont24[k].Msk[i]&(0x80>>j))	LCD_DrawPoint(x,y);//画一个点
								x++;
								if((x-x0)==24)
								{
									x=x0;
									y++;
									break;
								}
							}
						}
					}


				}
				continue;  //已经找到对应的字库则退出，防止因重复取模影响速度
			}

	LCD_Set_Window(0,0,lcddev.width,lcddev.height);//恢复窗口为全屏
}

//显示 32*32
void GUI_DrawFont32(uint16_t x, uint16_t y, uint8_t *s,uint8_t mode)
{
	uint8_t i,j;
	uint16_t k;
	uint16_t HZnum;
	uint16_t x0=x;
	HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);	//自动统计汉字数目
	for (k=0;k<HZnum;k++)
			{
			  if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
			  { 	LCD_Set_Window(x,y,32,32);
						LCD_WriteRAM_Prepare();
				    for(i=0;i<32*4;i++)
				    {
						for(j=0;j<8;j++)
				    	{
							if(!mode) //非叠加方式
							{
								if(tfont32[k].Msk[i]&(0x80>>j))	LCD_WriteRAM(POINT_COLOR);
								else LCD_WriteRAM(BACK_COLOR);
							}
							else
							{
								//POINT_COLOR=fc;
								if(tfont32[k].Msk[i]&(0x80>>j))	LCD_DrawPoint(x,y);//画一个点
								x++;
								if((x-x0)==32)
								{
									x=x0;
									y++;
									break;
								}
							}
						}
					}


				}
				continue;  //已经找到对应的字库则退出，防止因重复取模影响速度
			}

	LCD_Set_Window(0,0,lcddev.width,lcddev.height);//恢复窗口为全屏
}



//显示汉字或字符串
void Show_Str(uint16_t x, uint16_t y,uint8_t *str,uint8_t size,uint8_t mode)
{
	uint16_t x0=x;
  	uint8_t bHz=0;     //字符中文标志
    while(*str!=0)//数据未结束
    {
        if(!bHz)
        {
			if(x>(lcddev.width-size/2)||y>(lcddev.height-size))
			return;
	        if(*str>0x80)bHz=1;//汉字
	        else              //字符
	        {
		        if(*str==0x0D)//回车符
		        {
		            y+=size;
		            x=x0;
		            str++;
		        }
		        else
				{
					if(size>=24)//字库中没有包含12X24 16X32的英文字体,用8X16代替
					{
					LCD_ShowChar(x,y,*str,24,mode);
					x+=12; //字符,为全字的一半
					}
					else
					{
					LCD_ShowChar(x,y,*str,size,mode);
					x+=size/2; //字符,为全字的一半
					}
				}
				str++;

	        }
        }else//汉字
        {
				if(x>(lcddev.width-size)||y>(lcddev.height-size))
				return;
							bHz=0;//有汉字了
				if(size==32)
				GUI_DrawFont32(x,y,str,mode);
				else if(size==24)
				GUI_DrawFont24(x,y,str,mode);
				else
				GUI_DrawFont16(x,y,str,mode);

						str+=2;
						x+=size;//下一个汉字偏移
	        }
    }
}


//显示40*40图片 (DMA 加速)
void Gui_Drawbmp16(uint16_t x,uint16_t y,const unsigned char *p) //显示40*40图片
{
  	uint32_t total = 40 * 40;
	uint32_t remain = total;
	uint32_t batch;

	LCD_Set_Window(x,y,40,40);
	LCD_WriteRAM_Prepare();

	SPI_CS(0);
	SPI_DC(1);

	while (remain)
	{
		batch = (remain > (LCD_DMA_BUF_BYTES / 2)) ? (LCD_DMA_BUF_BYTES / 2) : remain;

		// 源数据 LSB-first (picL在前, picH在后) → MSB-first (高字节先发)
		for (uint32_t i = 0; i < batch; i++)
		{
			uint8_t picL = p[i * 2];
			uint8_t picH = p[i * 2 + 1];
			lcd_dma_buf[i * 2]     = picH;
			lcd_dma_buf[i * 2 + 1] = picL;
		}

		LCD_DMA_Send(lcd_dma_buf, batch * 2);
		p += batch * 2;
		remain -= batch;
	}

	SPI_CS(1);

	LCD_Set_Window(0,0,lcddev.width,lcddev.height);//恢复显示窗口为全屏
}

//居中显示
void Gui_StrCenter(uint16_t x, uint16_t y, uint8_t *str,uint8_t size,uint8_t mode)
{
	uint16_t x1;
	uint16_t len=strlen((const char *)str);
	if(size>16)
	{
		x1=(lcddev.width-len*(size/2))/2;
	}else
	{
		x1=(lcddev.width-len*8)/2;
	}

	Show_Str(x+x1,y,str,size,mode);
}


void Load_Drow_Dialog(void)
{
	LCD_Clear(WHITE);//清屏
 	POINT_COLOR=BLUE;//设置画笔为蓝色
	BACK_COLOR=WHITE;
	LCD_ShowString(lcddev.width-24,0,200,16,16,"RST");//显示字符串
  POINT_COLOR=RED;//设置画笔颜色
}
////////////////////////////////////////////////////////////////////////////////
//数据处理专用函数
//画水平线
//x0,y0:坐标
//len:线长度
//color:颜色
void gui_draw_hline(uint16_t x0,uint16_t y0,uint16_t len,uint16_t color)
{
	if(len==0)return;
	LCD_Fill(x0,y0,x0+len-1,y0,color);
}
//画实心圆
//x0,y0:中心
//r:半径
//color:颜色
void gui_fill_circle(uint16_t x0,uint16_t y0,uint16_t r,uint16_t color)
{
	uint32_t i;
	uint32_t imax = ((uint32_t)r*707)/1000+1;
	uint32_t sqmax = (uint32_t)r*(uint32_t)r+(uint32_t)r/2;
	uint32_t x=r;
	gui_draw_hline(x0-r,y0,2*r,color);
	for (i=1;i<=imax;i++)
	{
		if ((i*i+x*x)>sqmax)// draw lines from outside
		{
	 		if (x>imax)
			{
				gui_draw_hline (x0-i+1,y0+x,2*(i-1),color);
				gui_draw_hline (x0-i+1,y0-x,2*(i-1),color);
			}
			x--;
		}
		// draw lines from inside (center)
		gui_draw_hline(x0-x,y0+i,2*x,color);
		gui_draw_hline(x0-x,y0-i,2*x,color);
	}
}

//画一条直线
//(x1,y1),(x2,y2):直线的起始坐标
//size:直线的粗细程度
//color:直线的颜色
void lcd_draw_bline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,uint8_t size,uint16_t color)
{
	uint16_t t;
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	if(x1<size|| x2<size||y1<size|| y2<size)return;
	delta_x=x2-x1; //计算坐标差
	delta_y=y2-y1;
	uRow=x1;
	uCol=y1;
	if(delta_x>0)incx=1; //设置单步方向
	else if(delta_x==0)incx=0;//垂直线
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if(delta_y==0)incy=0;//水平线
	else{incy=-1;delta_y=-delta_y;}
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴
	else distance=delta_y;
	for(t=0;t<=distance+1;t++ )//画线输出
	{
		gui_fill_circle(uRow,uCol,size,color);//画点
		xerr+=delta_x ;
		yerr+=delta_y ;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}
