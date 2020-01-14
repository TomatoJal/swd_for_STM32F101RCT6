#ifndef __EVENT_H
#define __EVENT_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "typedef.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported Variable ---------------------------------------------------------*/
extern vu32 TimeCounter;
extern u8 programstate;
/* Exported functions ------------------------------------------------------- */
extern void SystemInit(void);
extern void PowerOnSelfTest(void);
extern void AutoProgMCUEvent(void);
extern void EraseMCUEvent(void);
extern void ProgramMCUEvent(void);
extern void VerifyMCUEvent(void);
extern u8 ProgramMCU(void);
extern void TIM4_Init(void);
extern void SecondEvent(void);
#endif

