#ifndef __ICT_COMMUNICATION_H
#define __ICT_COMMUNICATION_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "typedef.h"

/* Exported types ------------------------------------------------------------*/
enum MPB_CMD_CODE
{
    START_ERASE_MCU_REQ = 0x00,
    START_ERASE_MCU_RSP = START_ERASE_MCU_REQ | 0x80,

    ERASE_MCU_RESULT_REQ = 0x01,
    ERASE_MCU_RESULT_RSP = ERASE_MCU_RESULT_REQ | 0x80,

    START_PROGRAM_MCU_REQ = 0x02,
    START_PROGRAM_MCU_RSP = START_PROGRAM_MCU_REQ | 0x80,

    PROGRAM_MCU_RESULT_REQ = 0x03,
    PROGRAM_MCU_RESULT_RSP = PROGRAM_MCU_RESULT_REQ | 0x80,

    START_VERIFY_MCU_REQ = 0x04,
    START_VERIFY_MCU_RSP = START_VERIFY_MCU_REQ | 0x80,

    VERIFY_MCU_RESULT_REQ = 0x05,
    VERIFY_MCU_RESULT_RSP = VERIFY_MCU_RESULT_REQ | 0x80,

    CAL_RTC_REQ = 0x06,
    CAL_RTC_RSP = CAL_RTC_REQ | 0x80,

    START_AUTO_PROGRAM_MCU_REQ = 0x07,
    START_AUTO_PROGRAM_MCU_RSP = START_AUTO_PROGRAM_MCU_REQ | 0x80,

    AUTO_PROGRAM_MCU_RESULT_REQ = 0x08,
    AUTO_PROGRAM_MCU_RESULT_RSP = AUTO_PROGRAM_MCU_RESULT_REQ | 0x80,

    HOLD_MCU_RESET_REQ = 0x09,
    HOLD_MCU_RESET_RSP = HOLD_MCU_RESET_REQ | 0x80,

    RELEASE_MCU_RESET_REQ = 0x0A,
    RELEASE_MCU_RESET_RSP = RELEASE_MCU_RESET_REQ | 0x80,

    READ_MCU_FLASH_REQ = 0x0B,
    READ_MCU_FLASH_RSP = READ_MCU_FLASH_REQ | 0x80,

    WRITE_MCU_FLASH_REQ = 0x0C,
    WRITE_MCU_FLASH_RSP = WRITE_MCU_FLASH_REQ | 0x80,

    READ_METER_IMAGE_REQ = 0x0D,
    READ_METER_IMAGE_RSP = READ_METER_IMAGE_REQ | 0x80,

    WRITE_METER_IMAGE_REQ = 0x0E,
    WRITE_METER_IMAGE_RSP = WRITE_METER_IMAGE_REQ | 0x80,

    READ_ASSIT_IMAGE_REQ = 0x0F,
    READ_ASSIT_IMAGE_RSP = READ_ASSIT_IMAGE_REQ | 0x80,

    WRITE_ASSIT_IMAGE_REQ = 0x10,
    WRITE_ASSIT_IMAGE_RSP = WRITE_ASSIT_IMAGE_REQ | 0x80,

    START_ERASE_IMAGE_REQ = 0x11,
    START_ERASE_IMAGE_RSP = START_ERASE_IMAGE_REQ | 0x80,

    ERASE_IMAGE_RESULT_REQ = 0x12,
    ERASE_IMAGE_RESULT_RSP = ERASE_IMAGE_RESULT_REQ | 0x80,

    READ_VERSION_REQ = 0x13,
    READ_VERSION_RSP = READ_VERSION_REQ | 0x80,

};

enum MPB_DWN_ERROR_CODE
{
    NO_ERROR = 0,
    PKT_NUMBER_WRONG,
    FLASH_NOT_BLANK,
    BUFFER_FULL_PROGRAMMING,
} ;

enum
{
    ERR_PROG_MCU_BOOT_LOADER  = 1,
    ERR_PROG_MCU_DWNL_ASSIST  = 2,
    ERR_PROG_MCU_COMMU_ASSIST = 3,
    ERR_PROG_MCU_PROGRAMMING  = 4,
    ERR_PROG_MCU_SETWORDWRITE = 5,
};

/* Exported constants --------------------------------------------------------*/
#define MTRIMAGE_BASE_ADDRESS       0x00000000UL
#define MTRPROG_BASE_ADDRESS        (MTRIMAGE_BASE_ADDRESS + 128)

#define ASSIMAGE_BASE_ADDRESS       0x00080000UL
#define ASSPROG_BASE_ADDRESS        (ASSIMAGE_BASE_ADDRESS + 128)

#define CRC_ADDRESS                 0x00000038
#define DEV_ADDRESS                 0x00000040
#define MD5_ADDRESS                 0x00000018//0x28
#define LEN_ADDRESS                 0x00000010  

#define METER_IMAGE                 0x00
#define ASSIST_IMAGE                0x01
#define METER_IMAGE_SIZE            (1023 * 1024UL)
#define ASSIST_IMAGE_SIZE           (8 * 1024UL)

#define MD5_CALCULATING             0x5555AAAA
#define MD5_CALCULATED              0xAAAA5555

#define CRC_CALCULATING             0x5555AAAA
#define CRC_CALCULATED              0xAAAA5555


/* Exported macro ------------------------------------------------------------*/
/* Exported Variable ---------------------------------------------------------*/
extern u8 Version[17];

/* Exported functions --------------------------------------------------------*/
void ICT_CommuInit(void);
void ICT_CommuTimeout(void);
void ICT_MsgProcess(void);
void ICT_EraseImage(void);
void ICT_CalculateImageMD5(void);
void ICT_CalculateImageCRC(void);
u32  ICT_GetImageLength(u8 ImageName);
void ICT_ProgResultReq(u8 Command, u8 Result);
bool ICT_GetImageMd5(u8 *md5, u8 ImageName);
bool ICT_DataRequest(const u8 *src);
#endif

