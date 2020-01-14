#ifndef __PROGMCU_H
#define __PROGMCU_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "typedef.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define UUP_PROGRAM_START_ADDRESS    0x20003000
#define UUP_VOLTAGE_RANGE_ADDRESS    0xFFFF0000

/* Exported macro ------------------------------------------------------------*/
/* Exported Variable ---------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
void ProgMCUInit(void);
bool ProgMCUStart(void);
bool ProgMCUEraseAllFlash(void);
bool ProgMCUGoInstruction(u32 addr);
bool ProgMCUReadMemory(u32 addr, void *dat, u8 length);
bool SetFlashWriteByWord(void);
bool ProgMCUDwldAssist(void);
bool ProgMCUWriteMemory(u32 addr, void *dat, u8 length);
bool ProgMCUReadOutUnprotect(void);
bool ProgMCUReadOutProtect(void);
void ProgMCUTimeout(void);
bool ProgMCUWriteUnprotect(void);
bool ProgMCUWriteProtect(void);
#endif

