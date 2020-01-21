// Wrapper for target-specific flash loader code
#include <stdint.h>
#include "string.h"
#include "stm32f10x_flash.h"


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

#define CALRTC_START        0xD7000000
#define CALRTC_END          0x7D000000
#define CALRTC_ERROR        0x77000000
#define CALRTC_OK           0xDD000000

#define PACK_SIZE           16
#define RAM_SIZE            0xDD008000

// Functions in this file, called from the assembly wrapper
void Fl2FlashInitEntry(void);
void Fl2FlashEraseEntry(void);
void Fl2FlashEraseWriteEntry(void);
void Fl2FlashChecksumEntry(void);
void Fl2CalRTCEntry(void);
void FlashBreak(void);


uint16_t Crc16(uint8_t const *p, uint32_t len);
uint16_t Crc16_helper(uint8_t const *p, uint32_t len, uint16_t sum);

__root __no_init uint32_t target_status           @0x20000020;
#pragma location = 0x20000024
__root const char Version[16] = "BOOT_STM32_0001" ;           //@0x20000024;
__root __no_init uint8_t  flash[1024 * 256]       @0x08000000;
__root __no_init uint32_t image[1024 * PACK_SIZE / 4] @0x20004000;
__root __no_init double   ERR;
void Fl2FlashInitEntry()
{
    target_status = RAM_SIZE;
}

#define RAM_CHECK       1
#define RAND_WRITE      1
void Fl2FlashEraseEntry()
{
    uint32_t i=0;
    for(i = 0;i < sizeof(flash); i+=4)
    {
        if((*(uint32_t*)(&flash[i])) != 0xFFFFFFFF)
        {
            break;
        }
    }
    
    if(i != sizeof(flash))
    {
        FLASH_Unlock();
        FLASH_Status status = FLASH_BUSY;
        uint8_t timeout = 3;
        do{
            status = FLASH_EraseAllPages();
            timeout --;
        }while((status != FLASH_COMPLETE) && (timeout != 0));
        FLASH_Lock();
        target_status = ERASE_OK;  
    }
    else
    {
        target_status = ERASE_OK;  
    }
    while(target_status != ERASE_END);
}

 
// The erase-first flash write function -----------------------------------------

void Fl2FlashEraseWriteEntry()                                                                                                                                                                                                                                                                    
{
    uint32_t i; 
    uint32_t pack;

    /*获取包数*/
    target_status |= ((target_status & 0x0000FF00) >> 8);
    pack = (uint8_t)target_status;
    while((target_status & 0xFF000000) == PROGRAM_START)
    {
        /*需要写包*/
        if((uint8_t)target_status != (uint8_t)(target_status >> 8))
        {
            i = pack - (uint8_t)target_status;
            
            FLASH_Unlock();  
            FLASH_Status status = FLASH_BUSY;
            uint8_t timeout = 3;
            for(uint32_t j=0; j<(sizeof(image)/sizeof(image[0])); j++)
            {
                timeout = 3;
                do{
                    status = FLASH_ProgramWord(i*16*1024 + j*4, image[j]);
                    timeout --;
                }while((status != FLASH_COMPLETE) && (timeout != 0));
            }
            FLASH_Lock();
            target_status --;
        }
        /*写完成*/
        if(!(target_status & 0xFF))
          target_status = PROGRAM_OK;
    }
    while(target_status != PROGRAM_END);
}


void Fl2FlashChecksumEntry()
{
    /*校验*/
    uint8_t  lockval = 0;
    uint16_t val = 0;
    uint32_t size = target_status & 0xFFFFFF;
    val = Crc16(flash ,size);
    target_status = VERIFY_OK + val;  
    while(target_status != VERIFY_END);
}


uint16_t Crc16(uint8_t const *p, uint32_t len)
{
  uint8_t zero[2] = { 0, 0 };
  uint16_t sum = Crc16_helper(p, len, 0);
  return Crc16_helper(zero, 2, sum);
}

uint16_t Crc16_helper(uint8_t const *p, uint32_t len, uint16_t sum)
{
  while (len--)
  {
    int i;
    uint8_t byte = *p++;

    for (i = 0; i < 8; ++i)
    {
      uint32_t osum = sum;
      sum <<= 1;
      if (byte & 0x80)
        sum |= 1 ;
      if (osum & 0x8000)
        sum ^= 0x1021;
      byte <<= 1;
    }
  }
  return sum;
}
  

__root void FlashBreak()
{
  while(1)
  {   
    /*擦操作 0xA4000000*/
    if((target_status & 0xFF000000) == ERASE_START)     Fl2FlashEraseEntry();
    /*写操作 0xB5000f10*/
    if((target_status & 0xFF000000) == PROGRAM_START)   Fl2FlashEraseWriteEntry();
    /*校验操作 0xC60658F4 0xC60658EC 415988*/
    if((target_status & 0xFF000000) == VERIFY_START)    Fl2FlashChecksumEntry();
  }
}
