#ifndef __M25PXX_H
#define __M25PXX_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "typedef.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    M25_WIP = 0x01,         // write in process bit
    M25_WEL = 0x02,         // write enable latch bit
    M25_BP0 = 0x04,         // block protect bit 0
    M25_BP1 = 0x08,         // block protect bit 1
    M25_SRWD= 0x80,         // status register write protect
} m25pxx_status;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define SECTOR_SIZE     (64 * 1024UL)

/* Exported Variable ---------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
bool DataFlashTest(void);
void DataFlashInit(void);
void M25_ReadId(u8 *buf);
bool M25_ReadBytes(u32 addr, void *buf, u32 length);
bool M25_WriteBytes(u32 addr, const void *buf, u32 length);
bool M25_SectorErase(u32 addr);
bool M25_WholeChipErase(void);
u8   M25_ReadStatus(void);
bool M25_WriteStatus(u8 status);
bool M25_IsProgramming(void);

#endif

