#ifndef __SWD_PIN_H__
#define __SWD_PIN_H__

#include "stm32f10x_lib.h"
#include "typedef.h"

#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 

//IO�ڵ�ַӳ��
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //0x4001080C 
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C 
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //0x4001100C 
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //0x4001140C 
//#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //0x4001180C 
//#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //0x40011A0C    
//#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //0x40011E0C    

#define GPIOA_IDR_Addr    (GPIOA_BASE+8) //0x40010808 
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) //0x40010C08 
#define GPIOC_IDR_Addr    (GPIOC_BASE+8) //0x40011008 
#define GPIOD_IDR_Addr    (GPIOD_BASE+8) //0x40011408 
//#define GPIOE_IDR_Addr    (GPIOE_BASE+8) //0x40011808 
//#define GPIOF_IDR_Addr    (GPIOF_BASE+8) //0x40011A08 
//#define GPIOG_IDR_Addr    (GPIOG_BASE+8) //0x40011E08 
 
//IO�ڲ���,ֻ�Ե�һ��IO��!
//ȷ��n��ֵС��16!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //��� 
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //���� 

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //��� 
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //���� 

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //��� 
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //���� 

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //��� 
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //���� 

//#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //��� 
//#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //����
//
//#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //��� 
//#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //����
//
//#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //��� 
//#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //����

#if (1)

#define SWDIO_GPIO             GPIOA
#define SWDIO_PIN              9
#define SWDIO_GPIO_PIN         GPIO_Pin_9

#define SWCLK_GPIO             GPIOA//PA_10 RXD��ʱ����
#define SWCLK_PIN              10
#define SWCLK_GPIO_PIN         GPIO_Pin_10

#define SWDIO_SET_OUTPUT()     SWDIO_GPIO ->CRH &= 0XFFFFFF0F;\
                               SWDIO_GPIO ->CRH |= 0X00000030//�������
#define SWDIO_SET_INPUT()      SWDIO_GPIO ->CRH &= 0XFFFFFF0F;\
                               SWDIO_GPIO ->CRH |= 0X00000080//��������
                                 
#define SWCLK_SET_OUTPUT()     SWCLK_GPIO ->CRH &= 0XFFFFF0FF;\
                               SWCLK_GPIO ->CRH |= 0X00000300//ʱ����������� 
#else//��һ�Դ��ڽ�
                                 
#define SWDIO_GPIO             GPIOD
#define SWDIO_PIN              2
#define SWDIO_GPIO_PIN         GPIO_Pin_2

#define SWCLK_GPIO             GPIOC
#define SWCLK_PIN              12
#define SWCLK_GPIO_PIN         GPIO_Pin_12

#define SWDIO_SET_OUTPUT()     SWDIO_GPIO ->CRL &= 0XFFFFF0FF;\
                               SWDIO_GPIO ->CRL |= 0X00000300//�������
#define SWDIO_SET_INPUT()      SWDIO_GPIO ->CRL &= 0XFFFFF0FF;\
                               SWDIO_GPIO ->CRL |= 0X00000800//��������
                                 
#define SWCLK_SET_OUTPUT()     SWCLK_GPIO ->CRH &= 0XFFF0FFFF;\
                               SWCLK_GPIO ->CRH |= 0X00030000//ʱ�����������
#endif
                                 
#define SWCLK_SET()            PAout(SWCLK_PIN) = 1    
#define SWCLK_CLR()            PAout(SWCLK_PIN) = 0  
                                 
#define SWDIO_SET()            PAout(SWDIO_PIN) = 1
#define SWDIO_CLR()            PAout(SWDIO_PIN) = 0

#define SWDIO_IN()            (PAin(SWDIO_PIN)& 0x01U)//BITBAND_REG(PTB->PDIR, n)                               
#define SWDIO_OUT(n)          {if ( n ) SWDIO_SET(); else SWDIO_CLR();}                                 
                                     
/* pin interface */
void SW_PinInit(void);

#endif


