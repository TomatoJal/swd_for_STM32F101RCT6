/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "event.h"
#include "misc.h"
#include "m25pxx.h"
#include "progmcu.h"
#include "md5.h"
#include "ict_communication.h"
#include "dwn_assist.h"
#include "swd_pin.h"
#include "firmware.h"
#include "swd_dap.h"
#include "swd_app.h"
#include "measure_freq.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
ErrorStatus HSEStartUpStatus;
u8 ProgramBuffer[FLASH_PAGE_SIZE];
u32 TmpTime;//编程时时差用时
u32 SysTime;//系统总时间
u32 DowTime;//下载总时间
vu32 TimeCounter;
u8 programstate = 0;
/* Private function prototypes -----------------------------------------------*/
static void RCC_Configuration(void);
static void GetResetFlag(void);
static void EncryptProtection(void);
FLASH_Status Flash_WriteBytes(u32 addr, const void *dat, u32 length);

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : SystemInit
* Description    : Init the system.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void SystemInit(void)
{
    GetResetFlag();
    
    /* System Clocks Configuration */
    RCC_Configuration();
    
    DisAllInterrupt();
    
    EncryptProtection();
    DataFlashInit();
    ICT_CommuInit();
    TIM4_Init();
    MeasureFreqInit();
    MiscInit();
    
    SWD_PinInit();//swd引脚初始化
    Target_API_InfoInit();
    
    EnAllInterrupt();
   
}

/*******************************************************************************
* Function Name  : GetResetFlag
* Description    : Get the reset flag.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
// MCU Reset Source
// bit0 Low Power Reset
// bit1 Window Watchdog Reset
// bit2 Independent Watchdog Reset
// bit3 Software Reset
// bit4 POR/PDR Reset
// bit5 PIN Reset
#define RCC_PINRST          (1 << 10)
#define RCC_PORRST          (1 << 11)
#define RCC_SFTRST          (1 << 12)
#define RCC_IWDGRST         (1 << 13)
#define RCC_WWDGRST         (1 << 14)
#define RCC_LPWRRST         (1 << 15)
static u16 ResetSource;

static void GetResetFlag(void)
{
    u16 flag = (u16)(RCC->CSR >> 16);
    ResetSource = 0;

    // clear the reset flag
    RCC_ClearFlag();

    if(flag & RCC_IWDGRST)
    {
        ResetSource |= RCC_IWDGRST;
        flag = 0;
    }

    if(flag & RCC_PINRST)
    {
        if(flag & RCC_PORRST)
        {
            ResetSource |= RCC_PORRST;
        }
        else
        {
            ResetSource |= RCC_PINRST;
        }
    }
}
/*******************************************************************************
* Function Name  : TIM_Init
* Description    
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void TIM4_Init()
{
  TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
  
  SysTime = 0;
  
  TIM_TimeBaseInitStructure.TIM_Prescaler = 7199;//分频（72），记一个数0.1ms
  TIM_TimeBaseInitStructure.TIM_Period = 9;//1ms进入一次中断
  TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInitStructure.TIM_ClockDivision = 0x0;
  TIM_TimeBaseInit(TIM4,&TIM_TimeBaseInitStructure);
  TIM_ITConfig(TIM4,TIM_IT_Update,ENABLE);
  TIM_Cmd(TIM4,ENABLE);
  
  //中断优先级 NVIC 设置
  NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQChannel; //TIM4 中断
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; //先占优先级 0 级
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3; //从优先级 3 级
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ 通道被使能
  NVIC_Init(&NVIC_InitStructure);
  
  
}
/*******************************************************************************
* Function Name  : EncryptProtection
* Description    : Protect the program inside the chip, ensure that all chips have
                   different flash content.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
#define UNIQUE_ID_BASE_ADDR     0x1FFFF7E8
#define MD5FLAG_ADDR            0x08000bA4  /* It's the Md5Flag address. Please reffer to map file. */
#define MD5RESULT_ADDR          0x08000b94  /* It's the Md5Encrytion address. Please reffer to map file. */

const u8  Md5Encrytion[16] = {0xFF, 0xFF, 0xFF, 0xFF,
                             0xFF, 0xFF, 0xFF, 0xFF,
                             0xFF, 0xFF, 0xFF, 0xFF,
                             0xFF, 0xFF, 0xFF, 0xFF};
const u32 Md5Flag = 0xFFFFFFFF;

static void EncryptProtection(void)
{
    u8 Md5[32] = {0};
    u32 flag;
    MD5_CTX Encrypt;

    memcpy(Md5, Md5Encrytion, 12);
    memcpy(&Md5[12], &Md5Flag, 12);

    memcpy(Md5, (u8 *)UNIQUE_ID_BASE_ADDR, 12);
    memcpy(&Md5[12], __TIME__, 20);

    MD5Init(&Encrypt);
    MD5Update(&Encrypt, Md5, 32);
    MD5Final(Md5, &Encrypt);
    memcpy(&flag, (u8 *)MD5FLAG_ADDR, 4);
    return;
//    if(flag == 0xFFFFFFFF)
//    {
//        u32 Md5Prev = 0;
//        Flash_WriteBytes(MD5FLAG_ADDR, &Md5Prev, 4);
//        Flash_WriteBytes((u32)MD5RESULT_ADDR, Md5, 16);
//    }
//    else
//    {
//        memcpy(&Md5[16], (void *)MD5RESULT_ADDR, 16);
//
//        if(memcmp(Md5, &Md5[16], 16))
//        {
//            while(1);
//        }
//    }
}

/*******************************************************************************
* Function Name  : RCC_Configuration
* Description    : Configures the different system clocks.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void RCC_Configuration(void)
{
  /* RCC system reset(for debug purpose) */
  RCC_DeInit();

  NVIC_DeInit();

  /* Enable HSE */
    /* Enable HSE */
  RCC_HSEConfig(RCC_HSE_Bypass);
  //RCC_HSEConfig(RCC_HSE_ON);

  /* Wait till HSE is ready */
  HSEStartUpStatus = RCC_WaitForHSEStartUp();

  if(HSEStartUpStatus == SUCCESS)
  {
    /* Enable Prefetch Buffer */
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);

    /* Flash 1 wait state */
    FLASH_SetLatency(FLASH_Latency_1);

    /* HCLK = SYSCLK */
    RCC_HCLKConfig(RCC_SYSCLK_Div1);

    /* PCLK2 = HCLK */
    RCC_PCLK2Config(RCC_HCLK_Div1);

    /* PCLK1 = HCLK */
    RCC_PCLK1Config(RCC_HCLK_Div1);

    /* PLLCLK = 8MHz * 4 = 32 MHz */
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);

    /* Enable PLL */
    RCC_PLLCmd(ENABLE);

    /* Wait till PLL is ready */
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
    {
    }

    /* Select PLL as system clock source */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* Wait till PLL is used as system clock source */
    while(RCC_GetSYSCLKSource() != 0x08)
    {
    }
  }

#ifdef  VECT_TAB_RAM
  /* Set the Vector Table base location at 0x20000000 */
  NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x0);
#else  /* VECT_TAB_FLASH  */
  /* Set the Vector Table base location at 0x08000000 */
  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);
#endif

  /* Configure the NVIC Preemption Priority Bits */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

  /* Configure HCLK clock as SysTick clock source. */
  //SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
}

/*******************************************************************************
* Function Name  : PowerOnSelfTest
* Description    : When power on, the mcu will the itself.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void PowerOnSelfTest(void)
{

    DPRINTF("\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n");

    if(ResetSource & RCC_IWDGRST)
    {
        DPRINTF("\r\nThe watch dog has reset the MPB MCU!!!");
    }

    if(ResetSource & RCC_PORRST)
    {
        DPRINTF("\r\nThe power on has reset the MPB MCU!!!");
    }

    if(ResetSource & RCC_PINRST)
    {
        DPRINTF("\r\nThe reset pin has reset the MPB MCU!!!");
    }

    DPRINTF("\r\nThe MPB version is %s. Compiled time is %s, %s.", Version, __DATE__, __TIME__);

    if(HSEStartUpStatus != SUCCESS)
    {
        DPRINTF("\r\nThe MPB HSE has failed to startup!!! Please check!!!");
        while(1);
    }
}

/*******************************************************************************
* Function Name  : AutoProgMCUEvent
* Description    : Handle the programming MCU event.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
#define AUTO_PROGRAM_ERR_CODE_ERASE_FLASH   1
#define AUTO_PROGRAM_ERR_CODE_PROGRAM       2
#define AUTO_PROGRAM_ERR_CODE_VERIFICATION  3
#define AUTO_PROGRAM_ERR_CODE_UNPROTECTED   4

void AutoProgMCUEvent(void)
{
  u8 err;
  err = Target_API_AUTO();
  FEED_DOG();
  ICT_ProgResultReq(AUTO_PROGRAM_MCU_RESULT_REQ, err);
}

/*******************************************************************************
* Function Name  : EraseMCUEvent
* Description    : Handle the second event.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void EraseMCUEvent(void)
{
  u8 err;
  err = Target_API_Erase();
  FEED_DOG();
  ICT_ProgResultReq(ERASE_MCU_RESULT_REQ, err);
}


/*******************************************************************************
* Function Name  : ProgramMCUEvent
* Description    : Program the MCU event.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ProgramMCUEvent(void)
{
    u8 err;
    err = Target_API_PROGRAM();
    FEED_DOG();
    ICT_ProgResultReq(PROGRAM_MCU_RESULT_REQ, err);
}



/*******************************************************************************
* Function Name  : VerifyMCUEvent
* Description    : Handle the second event.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void VerifyMCUEvent(void)
{
    u8 err;
    err = Target_API_VERIFY();
    FEED_DOG();
    ICT_ProgResultReq(VERIFY_MCU_RESULT_REQ, err);
}

/*******************************************************************************
* Function Name  : DisAllInterrupt
* Description    : Disable all the interrupt.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static u16 DisCnt = 0;

void DisAllInterrupt(void)
{
    if(DisCnt == 0)
    {
        __SETPRIMASK();
    }

    DisCnt++;
}

/*******************************************************************************
* Function Name  : EnAllInterrupt
* Description    : Enable all the interrupt.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void EnAllInterrupt(void)
{
    DisCnt--;

    if(DisCnt == 0)
    {
        __RESETPRIMASK();
    }
}

/*******************************************************************************
* Function Name  : Flash_WriteBytes
* Description    : Write data to MCU flash.
* Input          : addr:    Mcu flash address
                   dat:     pionter to the data
                   length:  length of the data
* Output         : None
* Return         : FLASH_Status.
*******************************************************************************/
// Flash Keys
#define RDPRT_KEY       0x5AA5
#define FLASH_KEY1      0x45670123
#define FLASH_KEY2      0xCDEF89AB

// Flash Control Register definitions
#define FLASH_PG        0x00000001
#define FLASH_PER       0x00000002
#define FLASH_MER       0x00000004
#define FLASH_OPTPG     0x00000010
#define FLASH_OPTER     0x00000020
#define FLASH_STRT      0x00000040
#define FLASH_LOCK      0x00000080
#define FLASH_OPTWRE    0x00000100

// Flash Status Register definitions
#define FLASH_BSY       0x00000001
#define FLASH_PGERR     0x00000004
#define FLASH_WRPRTERR  0x00000010
#define FLASH_EOP       0x00000020

FLASH_Status Flash_WriteBytes(u32 addr, const void *dat, u32 length)
{
    FLASH_Status Status = FLASH_COMPLETE;
    u16 *src = (u16 *)dat;
    vu16 timeout;

    DisAllInterrupt();

    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;

    while(length)
    {
        if(*((vu16 *)addr) == *src)
        {
            goto NEXT_WORD;
        }

        timeout = 0xFFFF;

        // Programming Enabled
        FLASH->CR |=  FLASH_PG;

        // Program Half Word
        *((vu16 *)addr) = *src;
        while((FLASH->SR & FLASH_BSY) && (--timeout != 0));

        // Programming Disabled
        FLASH->CR &= ~FLASH_PG;

        // Check for Errors
        if((FLASH->SR & (FLASH_PGERR | FLASH_WRPRTERR)) || (timeout == 0))
        {
            // Failed
            FLASH->SR |= (FLASH_PGERR | FLASH_WRPRTERR);
            EnAllInterrupt();
            return FLASH_ERROR_PG;
        }

    NEXT_WORD:
        src++;
        addr += 2;
        length -= 2;
    }

    EnAllInterrupt();

    return Status;
}

/*******************************************************************************
* Function Name  : SecondEvent
* Description    : Handle the second event.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void SecondEvent(void)
{
//    CheckMeasureResult();
}