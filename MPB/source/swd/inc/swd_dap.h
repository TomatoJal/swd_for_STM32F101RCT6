#ifndef __SW_DAP_H__
#define __SW_DAP_H__
#include "swd_pin.h"
#include "debug_cm.h"
#include "stm32f10x_lib.h"
#include "typedef.h"

/****************MACRO**************/
#define RESULT_OK                   0
#define RESULT_ERROR                1
#define RESULT_TIMEOUT 0xFF
#define Check(x)                    if(x)return(RESULT_ERROR)

#define MAX_SWD_RETRY   1
#define TARGET_AUTO_INCREMENT_PAGE_SIZE    (0x400)

extern uint32 SWD_Transfer(uint8 request, uint32 *data);
extern void SWD_ResetCycle();
#endif


