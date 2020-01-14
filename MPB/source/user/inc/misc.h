#ifndef __MISC_H
#define __MISC_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "typedef.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define BOOT0_PIN           GPIO_Pin_12
#define BOOT0_PORT          GPIOA
#define MCU_RESET_PIN       GPIO_Pin_8
#define MCU_RESET_PORT      GPIOC

#define DBG_UART            UART5
/* Exported macro ------------------------------------------------------------*/
#define BOOT_FROM_FLASH()   GPIO_ResetBits(BOOT0_PORT, BOOT0_PIN)
#define BOOT_FROM_SYSMEM()  GPIO_SetBits(BOOT0_PORT, BOOT0_PIN)

#define HOLD_MCU_RESET()    GPIO_SetBits(MCU_RESET_PORT, MCU_RESET_PIN)
#define RELEASE_MCU_RESET() GPIO_ResetBits(MCU_RESET_PORT, MCU_RESET_PIN)

#define FEED_DOG()          IWDG_ReloadCounter()

/* Exported Variable ---------------------------------------------------------*/
extern bool FeedDog;
/* Exported functions --------------------------------------------------------*/
void MiscInit(void);
void MCUDelayMs(u16 ms);
void DPRINTF(const char *src, ...);

#endif

