#ifndef __DWN_ASSIST_H
#define __DWN_ASSIST_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "typedef.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    CMD_WRITE_REQ   = 'w',
    CMD_WRITE_RSP   = 'W',
    CMD_READ_REQ    = 'r',
    CMD_READ_RSP    = 'R',
    CMD_QUERY_MD5_REQ = 'q',
    CMD_QUERY_MD5_RSP = 'Q',
    CMD_RESET_DWN_REQ  = 's',
    CMD_RESET_DWN_RSP  = 'S',
    CMD_TEST_REQ    = 't',
    CMD_TEST_RSP    = 'T',
    CMD_BUF_EMPTY_REQ = 'e',
    CMD_BUF_EMPTY_RSP = 'E',
    CMD_BLANK_REQ = 'b',
    CMD_BLANK_RSP = 'B',
} CMD_CODE;
/* Exported constants --------------------------------------------------------*/
#define PACKET_SIZE             128UL
#define FLASH_PAGE_SIZE         2048UL
#define FLASH_START_ADDR        0x08000000UL

#define ASSIST_NO_ERROR             0
#define ASSIST_REQ_RSP_NOT_MATCH    0xFF
#define ASSIST_RSP_TIMEOUT          0xFE

/* Exported macro ------------------------------------------------------------*/
/* Exported Variable ---------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
void AssistInit(void);
bool AssistDataIndication(tTransceiverFSM *receiver, u8 dat);
bool AssistWriteFlash(const u8 *dat, u16 pkts, u8 *err_code);
bool AssistReadFlash(u8 *dat, u16 pkts, u8 *err_code);
bool AssistQueryMD5(u8 *err_code, u8 *md5, u32 *flag);
bool AssistWaitForBufEmpty(void);
bool AssistTest(void);
void AssistTimeout(void);

#endif

