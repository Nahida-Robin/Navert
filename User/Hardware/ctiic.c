#include "ctiic.h"
#include "delay.h"
#include <stm32f4xx.h>

/* SDA引脚方向切换 — 直接寄存器操作 (避免GPIO_Init开销) */
void CT_SDA_SetInput(void)
{
	GPIOC->MODER &= ~(3 << 4);          // PC2: MODER[4:5]=00: Input
	GPIOC->PUPDR = (GPIOC->PUPDR & ~(3 << 4)) | (1 << 4); // PUPDR[4:5]=01: Pull-up
}

void CT_SDA_SetOutput(void)
{
	GPIOC->MODER = (GPIOC->MODER & ~(3 << 4)) | (1 << 4); // PC2: MODER[4:5]=01: Output
	GPIOC->OTYPER &= ~(1 << 2);         // OTYPER[2]=0: Push-pull
	GPIOC->OSPEEDR = (GPIOC->OSPEEDR & ~(3 << 4)) | (2 << 4); // OSPEEDR[4:5]=10: High speed
	GPIOC->PUPDR = (GPIOC->PUPDR & ~(3 << 4)) | (1 << 4); // PUPDR[4:5]=01: Pull-up
}


//����I2C�ٶȵ���ʱ
void CT_Delay(void)
{
	Delay_us(5);
} 
//���ݴ���оƬIIC�ӿڳ�ʼ��
void CT_IIC_Init(void)
{					     
		//���ݴ����ӿڶ���
		//FT_INT  		= PA5  �ж�
		//CT_IIC_SDA  = PA4  ���� ���\����
		//CT_IIC_SCL  = PA6  ʱ��
		//FT_RST 		  = PA7	 ��λ
	
		__HAL_RCC_GPIOC_CLK_ENABLE();

		GPIO_InitTypeDef GPIO_InitStructure = {0};
		GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
		GPIO_InitStructure.Pull = GPIO_PULLUP;
		GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

		/* PC0(FT_INT) as input pull-up */
		GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
		GPIO_InitStructure.Pin = GPIO_PIN_0;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

//		RCC->APB2ENR|= 1<<2;    //PAʱ��ʹ��	   
//		RCC->APB2ENR|=1<<0;    //��������ʱ��							  
//		GPIOA->CRL&=0X00000FFF;//PC0~3
//		GPIOA->CRL|=0X33838000; 
//		GPIOA->ODR|=0X00F8;    //PC0~3 13 ȫ������  	
}
//����IIC��ʼ�ź�
void CT_IIC_Start(void)
{
	CT_SDA_OUT();     //sda�����
	CT_IIC_SDA(1);	  	  
	CT_IIC_SCL(1);
	Delay_us(30);
 	CT_IIC_SDA(0);//START:when CLK is high,DATA change form high to low 
	CT_Delay();
	CT_IIC_SCL(0);//ǯסI2C���ߣ�׼�����ͻ�������� 
}	  
//����IICֹͣ�ź�
void CT_IIC_Stop(void)
{
	CT_SDA_OUT();//sda�����
	CT_IIC_SCL(1);
	Delay_us(30);
	CT_IIC_SDA(0);//STOP:when CLK is high DATA change form low to high
	CT_Delay();
	CT_IIC_SDA(1);//����I2C���߽����ź�  
}
//�ȴ�Ӧ���źŵ���
//����ֵ��1������Ӧ��ʧ��
//        0������Ӧ��ɹ�
uint8_t CT_IIC_Wait_Ack(void)
{
	uint8_t ucErrTime=0;
	CT_SDA_IN();      //SDA����Ϊ����  
	CT_IIC_SDA(1);	   
	CT_IIC_SCL(1);
	CT_Delay();
	while(CT_READ_SDA)
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			CT_IIC_Stop();
			return 1;
		} 
		CT_Delay();
	}
	CT_IIC_SCL(0);//ʱ�����0 	   
	return 0;  
} 
//����ACKӦ��
void CT_IIC_Ack(void)
{
	CT_IIC_SCL(0);
	CT_SDA_OUT();
	CT_Delay();
	CT_IIC_SDA(0);
	CT_Delay();
	CT_IIC_SCL(1);
	CT_Delay();
	CT_IIC_SCL(0);
}
//������ACKӦ��		    
void CT_IIC_NAck(void)
{
	CT_IIC_SCL(0);
	CT_SDA_OUT();
	CT_Delay();
	CT_IIC_SDA(1);
	CT_Delay();
	CT_IIC_SCL(1);
	CT_Delay();
	CT_IIC_SCL(0);
}					 				     
//IIC����һ���ֽ�
//���شӻ�����Ӧ��
//1����Ӧ��
//0����Ӧ��			  
void CT_IIC_Send_Byte(uint8_t txd)
{                        
    uint8_t t;   
	CT_SDA_OUT(); 	    
    CT_IIC_SCL(0);//����ʱ�ӿ�ʼ���ݴ���
	CT_Delay();
	for(t=0;t<8;t++)
    {              
        CT_IIC_SDA((txd&0x80)>>7);
        txd<<=1; 	      
		CT_IIC_SCL(1); 
		CT_Delay();
		CT_IIC_SCL(0);	
		CT_Delay();
    }	 
} 	    
//��1���ֽڣ�ack=1ʱ������ACK��ack=0������nACK   
uint8_t CT_IIC_Read_Byte(unsigned char ack)
{
	uint8_t i,receive=0;
		CT_SDA_IN();//SDA����Ϊ����
	Delay_us(30);
	for(i=0;i<8;i++ )
	{ 
		CT_IIC_SCL(0); 	    	   
		CT_Delay();
		CT_IIC_SCL(1);	 
		receive<<=1;
		if(CT_READ_SDA)receive++;   
	}	  				 
	if (!ack)CT_IIC_NAck();//����nACK
	else CT_IIC_Ack(); //����ACK   
 	return receive;
}




























