/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "ict_communication.h"
#include "measure_freq.h"
#include "misc.h"
#include "firmware.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#define TIM3_CCR3_ADDR              0x4000043C
#define CCR3_BUFFER_LENGTH          15

#define RTC_WAVEFORM_CAPTURE_PIN    GPIO_Pin_0
#define RTC_WAVEFORM_CAPTURE_PORT   GPIOB

#define TIMEOUT_MEASUREFREQ      100000

#define TIM_PRESCALER            72                 //72分频
#define MEASURE_FREQ             (double)1          //待测频率
#define MEASURE_FREQ_ACC         (double)0.000001   //频率精度
#define MEASURE_FREQ_ERR         (double)0.000100   //合法频率范围
#define MEASURE_FREQ_ERR_ALLOW   (double)0.000347   //允许校准范围30s/d
#define MEASURE_OVERFLOWTIMES    15//溢出次数

#define UPPER_LIMIT             (72000000/TIM_PRESCALER/(MEASURE_FREQ-MEASURE_FREQ_ERR))//15873//7874//20000000//333333//7874      /* (32.000MHz / 32 / 511) */
#define LOWER_LIMIT             (72000000/TIM_PRESCALER/(MEASURE_FREQ+MEASURE_FREQ_ERR))//15384//7751//666666//200000//7751      /* (32.000MHz / 32 / 513) */
#define SAMPLE_NUMBER_LIMIT     8

/* Private variables ---------------------------------------------------------*/
u32  TIM3_CCR3Buffer[CCR3_BUFFER_LENGTH];
bool MeasureRTCFlag;

/* Private function prototypes -----------------------------------------------*/
void DMA_Configuration(void);

/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
* Function Name  : MeasureFreqInit
* Description    : Init some device to measure the RTC frequency.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
u32 TestLimitUpper,TestLimitLower,TestOverflow;    
void MeasureFreqInit(void)
{
    //NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef       TIM_ICStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // NVIC init
    /*NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 15;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);*/

    GPIO_InitStructure.GPIO_Pin  = RTC_WAVEFORM_CAPTURE_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(RTC_WAVEFORM_CAPTURE_PORT, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Prescaler     = TIM_PRESCALER - 1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period        = 0xffff;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_ICStructure.TIM_Channel     = TIM_Channel_3;
    TIM_ICStructure.TIM_ICPolarity  = TIM_ICPolarity_Rising;
    TIM_ICStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICStructure.TIM_ICFilter    = 0x00;

    TIM_ICInit(TIM3, &TIM_ICStructure);

    /* TIM3 IT enable */
    //TIM_ITConfig(TIM3, TIM_IT_CC3, ENABLE);

    DMA_Configuration();

    /* Enable DMA access data length. */
    TIM_DMAConfig(TIM3, TIM_DMABase_CCR3, TIM_DMABurstLength_2Bytes);

    /* TIM3 CC3 DMA Request Enable */
    TIM_DMACmd(TIM3, TIM_DMA_CC3, ENABLE);

    /* TIM3 enable counter */
    TIM_Cmd(TIM3, ENABLE);
    
    TestLimitUpper = UPPER_LIMIT;
    TestLimitLower = LOWER_LIMIT;
    TestOverflow = MEASURE_OVERFLOWTIMES;
}

/*******************************************************************************
* Function Name  : DMA_Configuration
* Description    : Config the DMA channel for the timer 3 channel 3.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void DMA_Configuration(void)
{
    /* DMA1 Channel2 Config */
    DMA_DeInit(DMA1_Channel2);

    DMA1_Channel2->CCR = (  DMA_DIR_PeripheralSRC | DMA_Mode_Normal
                        | DMA_PeripheralInc_Disable | DMA_MemoryInc_Enable
                        | DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord
                        | DMA_Priority_High | DMA_M2M_Disable);

    /*--------------------------- DMAy Channelx CNDTR Configuration ---------------*/
    /* Write to DMAy Channelx CNDTR */
    DMA1_Channel2->CNDTR = CCR3_BUFFER_LENGTH;

    /*--------------------------- DMAy Channelx CPAR Configuration ----------------*/
    /* Write to DMAy Channelx CPAR */
    DMA1_Channel2->CPAR  = TIM3_CCR3_ADDR;

    /*--------------------------- DMAy Channelx CMAR Configuration ----------------*/
    /* Write to DMAy Channelx CMAR */
    DMA1_Channel2->CMAR  = (u32)TIM3_CCR3Buffer;

    // DMA_Cmd(DMA1_Channel2, ENABLE);
    DMA1_Channel2->CCR  |= (u32)0x01;
}

/*******************************************************************************
* Function Name  : DMA_Reconfig
* Description    : Reconfig the DMA channel.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void DMA_Reconfig(void)
{
    DMA1_Channel2->CCR &= (u32)0;

    memset(TIM3_CCR3Buffer, 0x00, sizeof(TIM3_CCR3Buffer));

    DMA1_Channel2->CCR = (    DMA_DIR_PeripheralSRC | DMA_Mode_Normal
                            | DMA_PeripheralInc_Disable | DMA_MemoryInc_Enable
                            | DMA_PeripheralDataSize_Word | DMA_MemoryDataSize_Word
                            | DMA_Priority_VeryHigh | DMA_M2M_Disable);

    /*--------------------------- DMAy Channelx CNDTR Configuration ---------------*/
    /* Write to DMAy Channelx CNDTR */
    DMA1_Channel2->CNDTR = CCR3_BUFFER_LENGTH;

    /*--------------------------- DMAy Channelx CPAR Configuration ----------------*/
    /* Write to DMAy Channelx CPAR */
    DMA1_Channel2->CPAR = TIM3_CCR3_ADDR;

    /*--------------------------- DMAy Channelx CMAR Configuration ----------------*/
    /* Write to DMAy Channelx CMAR */
    DMA1_Channel2->CMAR = (u32)TIM3_CCR3Buffer;

#define DMA1_Channel2_IT_Mask    ((u32)0x000000F0)

    DMA1->IFCR |= DMA1_Channel2_IT_Mask;

    // DMA_Cmd(DMA1_Channel2, ENABLE);
    DMA1_Channel2->CCR |= (u32)0x01;
}

/*******************************************************************************
* Function Name  : MeasureFrequency
* Description    : None.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void MeasureFrequency(void)
{
    // detect if there are enough samples. samples must be greater than 1800.

    // check if the validity of the samples.
}

/*******************************************************************************
* Function Name  : CheckSamples
* Description    : Check all samples captured by the timer 3 channel3.
* Input          : u16 InputLen     Length of the sample array
                   u32 *TotalTime   Pointer to the total sample time
                   u16 *SampleNbr   Pointer to the total sample number
* Output         : None
* Return         : bool     TRUE,   the samples are valid.
                            FALSE,  the samples are not valid.
*******************************************************************************/
u32  TIM3_CCR3BufferTest[CCR3_BUFFER_LENGTH];
bool CheckSamples(u16 InputLen, u32 *TotalTime, u16 *SampleNbr)
{
    // disable the first ten samples
    u32 *src = TIM3_CCR3Buffer;
    u32 *Samples = TIM3_CCR3Buffer;
    u32 TimeDrift;
    u32 i = 0;
    *SampleNbr = 0;
    *TotalTime = 0;

    while(InputLen-- != 0)
    {
        TimeDrift = *(++src) - *Samples++;
        TIM3_CCR3BufferTest[i]=TimeDrift;
        i++;
        TimeDrift = (TimeDrift + 65536*MEASURE_OVERFLOWTIMES);
        if((TimeDrift <= UPPER_LIMIT) && (TimeDrift >= LOWER_LIMIT))
        {
            *SampleNbr += 1;
            *TotalTime += TimeDrift;
        }
        else
        {
            //*SampleNbr = 0;
            //*TotalTime = 0;
        }
    }

    if(*SampleNbr >= SAMPLE_NUMBER_LIMIT)
    {
        return TRUE;
    }

    return FALSE;
}

/*******************************************************************************
* Function Name  : CalculateFreq
* Description    : Calculate the frequency according to the total sample time
                   and total sample number.
* Input          : double *result   The measure result
                   u32 TotalTime    Total sample time
                   u16 SampleNbr    Total sample number
* Output         : None
* Return         : None
*******************************************************************************/
void CalculateFreq(double *result, u32 TotalTime, u16 SampleNbr)
{
    //Frequency = 1 / T = 1 / (((TotalTime) / (Number)) / (32.736M / 32))
    //*result = 1023000 * SampleNbr / TotalTime;
    *result = (double)(72000000/TIM_PRESCALER) * SampleNbr / TotalTime;

    DPRINTF("\r\nThe RTC frequency is %03.6fHz.", *result);
}

/*******************************************************************************
* Function Name  : CheckMeasureResult
* Description    : Calculate the frequency according to the total sample time
                   and total sample number.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
uint8 CheckMeasureResult(void)
{
    u32 TotalTime;
    u16 SampleNumber;
    double result;
    uint32 tmptime = 0;
    tmptime = SysTime;
    
      
    while(DMA1_Channel2->CNDTR != 0)
    {
        if((SysTime-tmptime) > TIMEOUT_MEASUREFREQ)        
        {
            return RESULT_ERROR;
        }
        
        if((SysTime-tmptime) % 3000 == 0)        
        {
            Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
        }
    }
        
    if(DMA1_Channel2->CNDTR == 0)
    {
        MeasureRTCFlag = FALSE;
        if(CheckSamples(CCR3_BUFFER_LENGTH, &TotalTime, &SampleNumber))
        {
            CalculateFreq(&result, TotalTime, SampleNumber);
            if(((result - MEASURE_FREQ)>MEASURE_FREQ_ERR_ALLOW)||
               ((MEASURE_FREQ - result)>MEASURE_FREQ_ERR_ALLOW))
            {
                return RESULT_ERROR;
            }
        }
        else
        {
             return RESULT_ERROR;
        }
    }
    
    TargetInfo.frequency = result;
    return RESULT_OK;
}

