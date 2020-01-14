#ifndef __MEASURE_FREQ_H
#define __MEASURE_FREQ_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "typedef.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported Variable ---------------------------------------------------------*/
#define RESULT_OK                   0
#define RESULT_ERROR                1

/* Exported functions --------------------------------------------------------*/
void MeasureFreqInit(void);
bool CheckSamples(u16 InputLen, u32 *TotalTime, u16 *SampleNbr);
void CalculateFreq(double *result, u32 TotalTime, u16 SampleNbr);
void DMA_Reconfig(void);
uint8 CheckMeasureResult(void);

#endif

