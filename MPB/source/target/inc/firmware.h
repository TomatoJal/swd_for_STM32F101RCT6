#ifndef _TARGET_H_
#define _TARGET_H_
#include "stm32f10x_lib.h"
#include "typedef.h"
#include "swd_dap.h"

//#define TARGET_RAM_START    TargetInfo.RAM_start
//#define TARGET_RAM_SIZE     TargetInfo.RAM_size
//#define TARGET_BOOT_SIZE    TargetInfo.bootloader_size
//#define TARGET_STATUS_ADDR  TargetInfo.status_addr
//#define TARGET_STATUS       TargetInfo.status
//#define TARGET_ENTRY        TargetInfo.entry_addr


#define DOWNLOAD_DONE          0
#define DOWNLOAD_ERROR_INIT    1
#define DOWNLOAD_ERROR_BOOT    2
#define DOWNLOAD_ERROR_GETINFO 3
#define DOWNLOAD_ERROR_ERASE   4
#define DOWNLOAD_ERROR_PROGRAM 5
#define DOWNLOAD_ERROR_VERIFY  6
#define DOWNLOAD_ERROR_RESET   7
#define CalRTC_ERROR           8 
#define CalRTC_ERROR_CHECK     9
#define DOWNLOAD_ERROR_UNKNOW  0xFF

#define ERASESTATES         4
#define WRITESTATES         4

#define ERASE_START         0xA4000000
#define ERASE_END           0x4A000000
#define ERASE_ERROR         0x44000000
#define ERASE_OK            0xAA000000
  
#define PROGRAM_START       0xB5000000
#define PROGRAM_END         0x5B000000
#define PROGRAM_ERROR       0x55000000
#define PROGRAM_OK          0xBB000000

#define VERIFY_START        0xC6000000
#define VERIFY_END          0x6C000000
#define VERIFY_ERROR        0x66000000
#define VERIFY_OK           0xCC000000

#define TARGET_RAMGOT       0xDD000000
#define DEVMAXNUM   255

#define CORE_CM0  0
#define CORE_CM3  3
#define CORE_CM4  4


/*目标信息*/
typedef struct{
  uint32 bootloader_size;//BootLoader大小
  uint32 entry_addr;
  uint32 RAM_start;
  uint32 RAM_size;
  uint32 image_num;
  uint32 status_addr;
  uint32 status;//低16位放写入包数，高16位写状态量
  uint32 freq_addrH;
  uint32 freq_addrL;
  double frequency;
}tTargetInfo;


/*捕获目标转换表*/
typedef struct{
  char DevName[6];//目标名称从Image名中获取
  tTargetInfo TargetInfoList;
}tCaptureTargetList;

extern tTargetInfo TargetInfo;
extern tCaptureTargetList CaptureTargetList [DEVMAXNUM];

extern uint8  Target_API_InfoInit(void);
extern uint8  Target_API_Erase(void);
extern uint8  Target_API_PROGRAM(void);
extern uint8  Target_API_VERIFY(void);
extern uint8  Target_API_AUTO(void);
extern uint16 Crc16_helper(uint8 const *p, uint32 len, uint16 sum);
#endif