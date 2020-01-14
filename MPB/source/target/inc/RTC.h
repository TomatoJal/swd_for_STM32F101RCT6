#ifndef _RTC_H_
#define _RTC_H_
#include "stm32f10x_lib.h"

#define  CAL_RTC_CHECK          1
#define  ARR2U32(x)             (uint32)(*x+(*(x+1)<<8)+(*(x+2)<<16)+(*(x+3)<<24))
#define  ARR2U16(x)             (uint16)(*x+(*(x+1)<<8))

#define  PIN_SEC_CLK            8

#define  TBS_TBSCON_ADDRESS     0x4000E000
#define  TBS_TMPDAT_ADDRESS     (0x4000E000 + 0x0C)

#define  CMU_BASE_ADDRESS       0x4000F000
#define  CMU_INFOLOCK_A_ADDRESS (0x4000F000 + 0x50)
#define  CMU_INFOLOCK_UnLocked  ((uint32)0xf998)
#define  CMU_INFOLOCK_Locked    ((uint32)0x0000)

#define  GPIOC_BASE_ADDRESS     0x40011200
#define  GPIOC_IOCFG_ADDRESS    (0x40011200 + 0x00)
#define  GPIOC_AFCFG_ADDRESS    (0x40011200 + 0x04)
#define  GPIOC_PTUP_ADDRESS     (0x40011200 + 0x0C)
#define  GPIOC_PTOD_ADDRESS     (0x40011200 + 0x20)

#define  RTC_BASE_ADDRESS       0x4000C000
#define  RTC_RTCCON_ADDRESS     (0x4000C000 + 0x00)                           
#define  RTC_DFAH_ADDRESS       (0x4000C000 + 0x50) 
#define  RTC_DFAL_ADDRESS       (0x4000C000 + 0x54) 
#define  RTC_DFBH_ADDRESS       (0x4000C000 + 0x58) 
#define  RTC_DFBL_ADDRESS       (0x4000C000 + 0x5C) 
#define  RTC_DFCH_ADDRESS       (0x4000C000 + 0x60) 
#define  RTC_DFCL_ADDRESS       (0x4000C000 + 0x64) 
#define  RTC_DFDH_ADDRESS       (0x4000C000 + 0x68) 
#define  RTC_DFDL_ADDRESS       (0x4000C000 + 0x6C) 
#define  RTC_DFEH_ADDRESS       (0x4000C000 + 0x70) 
#define  RTC_DFEL_ADDRESS       (0x4000C000 + 0x74) 
#define  RTC_TOFF_ADDRESS       (0x4000C000 + 0x78)
#define  RTC_MCON01_ADDRESS     (0x4000C000 + 0x7C) 
#define  RTC_MCON23_ADDRESS     (0x4000C000 + 0x80) 
#define  RTC_MCON45_ADDRESS     (0x4000C000 + 0x84) 
#define  RTC_DFIH_ADDRESS       (0x4000C000 + 0x88) 
#define  RTC_DFIL_ADDRESS       (0x4000C000 + 0x8C) 

#define  INFOBLOCK_ADDRESS      0x00100000
#define  TOFF_OFFSET            0x012C
#define  TEMP_OFFSET            0x015E
#define  CODE_OFFSET            0x015C
#define  DFAH_OFFSET            0x0104
#define  DFAL_OFFSET            0x0108
#define  DFBH_OFFSET            0x010C
#define  DFBL_OFFSET            0x0110
#define  DFCH_OFFSET            0x0114
#define  DFCL_OFFSET            0x0118
#define  DFDH_OFFSET            0x011C
#define  DFDL_OFFSET            0x0120
#define  DFEH_OFFSET            0x0124
#define  DFEL_OFFSET            0x0128
#define  MCON01_OFFSET          0x0130
#define  MCON23_OFFSET          0x0134
#define  MCON45_OFFSET          0x0138
#define  CHECKSUM_OFFSET        0x013C
extern uint8 CalRTC_Process();
#if (CAL_RTC_CHECK == 1)
extern uint8 CalRTC_ProcessCheck();   
#endif
#endif