#ifndef __HT6XXX_RTC_H__
#define __HT6XXX_RTC_H__

#ifdef __cplusplus
extern "C" {
#endif 
#include "ht6xxx.h"  
  
#define ADDRESS_DFAH        (HT_INFO_BASE+0x0104)
#define ADDRESS_DFAL        (HT_INFO_BASE+0x0108)
#define ADDRESS_DFBH        (HT_INFO_BASE+0x010C)
#define ADDRESS_DFBL        (HT_INFO_BASE+0x0110)
#define ADDRESS_DFCH        (HT_INFO_BASE+0x0114)
#define ADDRESS_DFCL        (HT_INFO_BASE+0x0118)
#define ADDRESS_DFDH        (HT_INFO_BASE+0x011C)
#define ADDRESS_DFDL        (HT_INFO_BASE+0x0120)
#define ADDRESS_DFEH        (HT_INFO_BASE+0x0124)
#define ADDRESS_DFEL        (HT_INFO_BASE+0x0128)
#define ADDRESS_TOFF        (HT_INFO_BASE+0x012C)
#define ADDRESS_MCON01      (HT_INFO_BASE+0x0130)
#define ADDRESS_MCON23      (HT_INFO_BASE+0x0134)
#define ADDRESS_MCON45      (HT_INFO_BASE+0x0138)
#define ADDRESS_CHECKSUM    (HT_INFO_BASE+0x013C)
#define ADDRESS_TEMP        (HT_INFO_BASE+0x015E)
#define ADDRESS_CODE        (HT_INFO_BASE+0x015C)

#define DEFAULT_DFAH        0x0000
#define DEFAULT_DFAL        0x0000
#define DEFAULT_DFBH        0x007F
#define DEFAULT_DFBL        0xDB61
#define DEFAULT_DFCH        0x007E
#define DEFAULT_DFCL        0xDBAC
#define DEFAULT_DFDH        0x0000
#define DEFAULT_DFDL        0x4ACF
#define DEFAULT_DFEH        0x007F
#define DEFAULT_DFEL        0xFA88
#define DEFAULT_TOFF        0
#define DEFAULT_MCON01      0x2000
#define DEFAULT_MCON23      0x0588
#define DEFAULT_MCON45      0x4488
  
//#define TEMP_GET_TIMES      5
//extern void HT_RTC_ToffCheck(void);
//extern void HT_RTC_RTCInfoCheck(void); 
//extern void HT_RTC_Calerr(void);
#ifdef __cplusplus
}
#endif

#endif