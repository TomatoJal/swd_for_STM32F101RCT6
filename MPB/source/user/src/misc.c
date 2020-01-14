/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "misc.h"
#include "main.h"
#include "event.h"
#include "progmcu.h"
#include "dwn_assist.h"
#include "ict_communication.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
tTransceiverFSM DebugSnd;
//tTransceiverFSM DebugRcv;
static vu16 DelayMs;
bool FeedDog;

/* Private function prototypes -----------------------------------------------*/
static void DebugUartInit(void);
static void TIM2_Init(void);
static void MY_PRINTF(const void *src, u16 length);
void TIM2_ISR(void);
void DebugUARTInterrupt(void);

/*
u8   tou8(u8 c);
u8   toasc(u8 c);
void Hexes2Asces(void *des, const void *src, u32 len);
void Asces2Hexes(void *des, const void *src, u32 len);
*/
/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
* Function Name  : MiscInit
* Description    : Init miscellaneous device.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void MiscInit(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);

    // initialize
    GPIO_InitStructure.GPIO_Pin = BOOT0_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOOT0_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = MCU_RESET_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MCU_RESET_PORT, &GPIO_InitStructure);

    HOLD_MCU_RESET();
    BOOT_FROM_SYSMEM();

    DebugUartInit();

    TIM2_Init();

#ifdef RELEASE
    /* Enable write access to IWDG_PR and IWDG_RLR registers */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    /* IWDG counter clock: 40KHz(LSI) / 64 = 0.625 KHz */
    IWDG_SetPrescaler(IWDG_Prescaler_64);
    IWDG_SetReload(349);
    IWDG_ReloadCounter();

    /* Enable IWDG (the LSI oscillator will be enabled by hardware) */
    IWDG_Enable();
#endif
}

/*******************************************************************************
* Function Name  : TIM2_Init
* Description    : Init timer 2 interrupt.
* Input          : None
* Output         : 1ms interrupt
* Return         : None
*******************************************************************************/
static void TIM2_Init(void)
{
    /* ---------------------------------------------------------------
    TIM2 Configuration:
    TIM2CLK = 32 MHz, Prescaler = 31, TIM2 counter clock = 1.0 MHz
    --------------------------------------------------------------- */
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    // RCC  init
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_TimeBaseStructure.TIM_Prescaler     = 31;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period        = 999;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* TIM2 IT enable */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* TIM2 enable counter */
    TIM_Cmd(TIM2, ENABLE);
}

/*******************************************************************************
* Function Name  : MCUDelayMs
* Description    : Delay for some miliseconds.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void MCUDelayMs(u16 ms)
{
    DisAllInterrupt();
    DelayMs = ms;
    EnAllInterrupt();

    while(0 != DelayMs){}
}

/*******************************************************************************
* Function Name  : TIM2_Interrupt
* Description    : Timer 2 interrupt handler.
* Input          : None
* Output         : 1ms interrupt
* Return         : None
*******************************************************************************/
void TIM2_ISR(void)
{
    static u16 _100msCnt  = 50;
    static u16 _1000msCnt = 500;
    //if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    if((TIM2->SR & (TIM_IT_Update)) != 0)
    {
        //TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        TIM2->SR &= ~TIM_IT_Update;

        ProgMCUTimeout();
        ICT_CommuTimeout();
        AssistTimeout();

        if(DelayMs)
        {
            DelayMs--;
        }

        if(++_100msCnt >= 100)
        {
            _100msCnt = 0;
            FeedDog = TRUE;
        }
        
        if(++_1000msCnt >= 1000)
        {
            _1000msCnt = 0;
            gEventFlag.Bits.Second = TRUE;
        }    
        TimeCounter++;
    }
}

/*******************************************************************************
* Function Name  : InitDebugUART
* Description    : Init the UART for debug interface.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void DebugUartInit(void)
{
    NVIC_InitTypeDef  NVIC_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* Enable UART5 clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);

    /* Enable the UART5 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 15;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    /* Configure UART5 Tx (PC.12) as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed= GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* Configure UART5 Rx (PD.2) as input floating */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    /* UART5 configuration ----------------------------------------------------*/
    /* UART5 configured as follow:
        - BaudRate = 115200 baud
        - Word Length = 8 Bits
        - One Stop Bit
        - EVEN parity
        - Receive and transmit enabled
        - Hardware flow control disabled (RTS and CTS signals)
    */
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    USART_Init(DBG_UART, &USART_InitStructure);
    USART_Cmd(DBG_UART, ENABLE);
}

/*******************************************************************************
* Function Name  : DebugUARTInterrupt
* Description    : Debug USART interrupt handler.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void DebugUARTInterrupt(void)
{
    //u8 tmpchar;

    //if(USART_GetITStatus(DBG_USART, USART_IT_RXNE) != RESET)
    //{
        //tmpchar = USART_ReceiveData(DBG_USART);
    //}

    //if(USART_GetITStatus(DBG_UART, USART_IT_TXE) != RESET)
    if((DBG_UART->SR & (1UL << 7)) != 0)
    {
        if(DebugSnd.FrameLen != 0)
        {
            DebugSnd.FrameLen--;
            DebugSnd.Timeout = 20;
            //USART_SendData(DBG_UART, DebugSnd.buf[DebugSnd.ptr++]);
            DBG_UART->DR = DebugSnd.buf[DebugSnd.ptr++];
        }
        else
        {
            DebugSnd.status  = FALSE;
            DebugSnd.Timeout = 0;
            //USART_ITConfig(DBG_UART, USART_IT_TXE, DISABLE);
            DBG_UART->CR1 &= ~(1UL << 7);
        }
    }
}

/*******************************************************************************
* Function Name  : DPRINTF
* Description    : Print the debug information.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
const s8 DbgBufFull[] = "\r\nThe debug buffer is full!!!";
void DPRINTF(const char *fmt, ...)
{
    char PrintLog[256];
    s32 number;
    va_list args;
    return;
    va_start(args, fmt);

    number = vsnprintf(PrintLog, sizeof(PrintLog) - 1, fmt, args);

    if(number == EOF)
    {
        MY_PRINTF(DbgBufFull, sizeof(DbgBufFull));
        goto DPRINTF_END;
    }
    else
    {
        if(DebugSnd.ptr + number > sizeof(DebugSnd.buf))
        {
            MY_PRINTF(DbgBufFull, sizeof(DbgBufFull));
            DebugSnd.ptr = 0;
            DebugSnd.FrameLen = 0;
            goto DPRINTF_END;
        }
    }

    fmt = PrintLog;

    DisAllInterrupt();
    if(DebugSnd.FrameLen != 0)
    {
        strcpy((char *)&DebugSnd.buf[DebugSnd.FrameLen + DebugSnd.ptr], fmt);
        DebugSnd.FrameLen += strlen((const char *)fmt);
    }
    else
    {
        DebugSnd.ptr = 0;
        strcpy((char *)DebugSnd.buf, fmt);
        DebugSnd.FrameLen = strlen(fmt);
        USART_ITConfig(DBG_UART, USART_IT_TXE, ENABLE);
    }
    EnAllInterrupt();

DPRINTF_END:
    va_end(args);
}

/*******************************************************************************
* Function Name  : MY_PRINTF
* Description    : Print the debug information through the UART.
* Input          : src, the debug information.
                   length, the length of the debug information.
* Output         : None
* Return         : None
*******************************************************************************/
static void MY_PRINTF(const void *src, u16 length)
{
    u8 *des = (u8 *)src;

    DisAllInterrupt();
    while(length--)
    {
        USART_SendData(DBG_UART, *des++);
        while (USART_GetFlagStatus(DBG_UART, USART_FLAG_TXE) == RESET);
    }
    EnAllInterrupt();
}

#if 0
/*******************************************************************************
* Function Name  : Hexes2Asces
* Description    : Translate the hex bytes into ascii codes.
* Input          : src      pointer to the hex bytes.
                   len      length of the hex bytes.
* Output         : des      pointer to the ascii codes.
* Return         : None
*******************************************************************************/
void Hexes2Asces(void *des, const void *src, u32 len)
{
    u8 *ptr1 = (u8 *)des;
    u8 *ptr2 = (u8 *)src;

    while(len--)
    {
        *ptr1++ = toasc(*ptr2 >> 4);
        *ptr1++ = toasc(*ptr2 & 0x0F);
        ptr1++;
    }
}

/*******************************************************************************
* Function Name  : Asces2Hexes
* Description    : Translate the ascii codes into hex bytes.
* Input          : src      pointer to ascii codes.
                   len      length of the ascii codes.
* Output         : des      pointer to the hex bytes.
* Return         : None
*******************************************************************************/
void Asces2Hexes(void *des, const void *src, u32 len)
{
    u8 *ptr1 = (u8 *)des;
    u8 *ptr2 = (u8 *)src;

    while(len--)
    {
        *ptr1  = tou8(*ptr2++) << 4;
        *ptr1 |= tou8(*ptr2++);
        ptr1++;
    }
}

/*******************************************************************************
* Function Name  : tou8
* Description    : Translate an ascii codea into a half byte.
* Input          : c    half byte input.
* Output         : None
* Return         : u8   the half byte. If the input is illegal, return 0.
*******************************************************************************/
u8 tou8(u8 c)
{
    if(c >= '0' && c <= '9')    return c - '0';
    if(c >= 'A' && c <= 'F')    return c - 'A' + 10;
    if(c >= 'a' && c <= 'f')    return c - 'a' + 10;

    return 0;
}

/*******************************************************************************
* Function Name  : toasc
* Description    : Translate a half byte into ascii code.
* Input          : c    half byte input.
* Output         : None
* Return         : u8   the ascii code. If the input is illegal, return 0.
*******************************************************************************/
u8 toasc(u8 c)
{
    if(c >= 16)   return 0;
    if(c >= 10)   return c + 'A' - 10;

    return c + '0';
}
#endif

