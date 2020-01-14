// Wrapper for target-specific flash loader code
#include <stdint.h>
#include "string.h"
#include "ht6xxx_flash.h"
#include "ht6xxx_cmu.h"
#include "ht6xxx_RTC.h"

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

#define PACK_SIZE           32
#define RAM_SIZE            0xDD010000

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
__root const char Version[16] = "BOOT_HT6x3x_0003" ;           //@0x20000024;
__root __no_init uint32_t  rand[16];
__root __no_init uint8_t  flash[1024 * 512]       @0x00000000;
__root __no_init uint32_t image[1024 * PACK_SIZE / 4] @0x20008000;
__root __no_init double   ERR;
void Fl2FlashInitEntry()
{
    __disable_irq(); 
    CMU_InitTypeDef cmu_init;
    cmu_init.CPUDiv = CPUDiv2;
    cmu_init.SysClkSel = SysPLL;//22M   
    HT_CMU_Init(&cmu_init);
	target_status = RAM_SIZE;
}

#define RAM_CHECK       1
#define RAND_WRITE      1
void Fl2FlashEraseEntry()
{
    uint32_t i=0;
    uint32_t save=0;
#if (RAM_CHECK == 1)   
    uint32_t bootSize = (target_status&0x00FFFFFF);
    for(i = 0;i < (64*1024 - bootSize); i+=4)
	{    
        save = (*(uint32_t*)(0x20000000+bootSize+i));
        (*(uint32_t*)(0x20000000+bootSize+i)) = 0xFFFFFFFF;
        if((*(uint32_t*)(0x20000000+bootSize+i)) != 0xFFFFFFFF)
        {
            target_status = ERASE_ERROR;
            while(target_status != ERASE_END);
        }
        (*(uint32_t*)(0x20000000+bootSize+i)) = 0;
        if((*(uint32_t*)(0x20000000+bootSize+i)) != 0)
        {
            target_status = ERASE_ERROR;
            while(target_status != ERASE_END);
        }
        (*(uint32_t*)(0x20000000+bootSize+i)) = 0xFFFFFFFF;
        if((*(uint32_t*)(0x20000000+bootSize+i)) != 0xFFFFFFFF)
        {
            target_status = ERASE_ERROR;
            while(target_status != ERASE_END);
        }
        (*(uint32_t*)(0x20000000+bootSize+i)) = save;
    }
#endif
    
    for(i = 0;i < sizeof(flash); i+=4)
	{
		if((*(uint32_t*)(&flash[i])) != 0xFFFFFFFF)
        {
            break;
        }
    }
    for(i = 0;i < sizeof(flash); i+=4)
	{
		if((*(uint32_t*)(&flash[i])) != 0xFFFFFFFF)
        {
            break;
        }
    }
    
    if(i != sizeof(flash))
    {
        HT_Flash_ChipErase();
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
        HT_Flash_WordWrite(image, i*32*1024, 8*1024);
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
    CMU_InitTypeDef cmu_init;
    cmu_init.CPUDiv = CPUDiv1;
    cmu_init.SysClkSel = SysPLLX2;//22M   
    HT_CMU_Init(&cmu_init);
    val = Crc16(flash ,size);
    target_status = VERIFY_OK + val;  
    
#if 0
    /*锁片*/
    HT_Flash_ByteWrite(&lockval,0x00000FC1,1);
    HT_Flash_ByteRead(&lockval,0x00000FC2,1);
    lockval &= 0x0F;
    HT_Flash_ByteWrite(&lockval,0x00000FC2,1);
#endif
    while(target_status != VERIFY_END);
}

#define INFOBLOCKSIZE    1024
#define RTC_INFOSIZE    (10+1+3+1)
void Fl2CalRTCEntry()
{
    uint32_t saveRTCInfo[RTC_INFOSIZE];
    uint32_t saveInfo[INFOBLOCKSIZE/4];
    uint8_t  i;
    uint32_t temp;
    CMU_InitTypeDef cmu_init;
    cmu_init.CPUDiv = CPUDiv1;
    cmu_init.SysClkSel = SysPLLX2;//22M   
    HT_CMU_Init(&cmu_init);
    
#if (RAND_WRITE == 1)
    HT_CMU_ClkCtrl0Config(CMU_CLKCTRL0_ARGEN,ENABLE);
    HT_RAND->RANDSTR |= 0x8000;
    HT_RAND->RANDSTR |= RAND_RANDSTR_BACKEN;
    HT_RAND->RANDSTR |= 2;
    HT_RAND->RANDSTR |= (uint16_t)2 << 8;  
#endif
    
    saveRTCInfo[0]  = (image[0] >> 16);//DFAH
    saveRTCInfo[1]  = (uint16_t)image[0];//DFAL
    saveRTCInfo[2]  = (image[1] >> 16);//DFBH
    saveRTCInfo[3]  = (uint16_t)image[1];//DFBL
    saveRTCInfo[4]  = (image[2] >> 16);//DFCH
    saveRTCInfo[5]  = (uint16_t)image[2];//DFCL
    saveRTCInfo[6]  = (image[3] >> 16);//DFDH
    saveRTCInfo[7]  = (uint16_t)image[3];//DFDL
    saveRTCInfo[8]  = (image[4] >> 16);//DFEH
    saveRTCInfo[9]  = (uint16_t)image[4];//DFEL
    saveRTCInfo[10] = (uint16_t)image[5];//Toff
    saveRTCInfo[11] = (uint16_t)image[6];//MCON01    
    saveRTCInfo[12] = (uint16_t)image[7];//MCON23    
    saveRTCInfo[13] = (uint16_t)image[8];//MCON45   
    saveRTCInfo[14] = 0;//CHECKSUM
    
    for(i=0;i<(RTC_INFOSIZE-1);i++)
    {
        saveRTCInfo[14] += saveRTCInfo[i];
    }
    memcpy(saveInfo, (uint32_t*)HT_INFO_BASE, sizeof(saveInfo));
    memcpy(&saveInfo[0x0104/4], saveRTCInfo, RTC_INFOSIZE*4);

#if (RAND_WRITE == 1)
    for(i = 0; i < 16; i++)
    {
        HT_RAND->RANDSTR |= RAND_RANDSTR_START;
        while((HT_RAND->RANDSTR & RAND_RANDSTR_START) != 0);
        saveInfo[0x300/4 + i] = HT_RAND->RANDDAT;
        temp = saveInfo[0x300/4 + i];
        HT_Flash_WordWrite(&temp ,0x00002FC0+i*4, 1);
    }
#endif
    
    HT_CMU->INFOLOCK_A = CMU_INFOLOCK_UnLocked;
    HT_Flash_PageErase(HT_INFO_BASE);
    HT_Flash_WordWrite(saveInfo,HT_INFO_BASE, 1024/4);
    HT_CMU->INFOLOCK_A = CMU_INFOLOCK_Locked;

    target_status = CALRTC_OK;    
    while(target_status != CALRTC_END);
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
    /*校RTC操作 0xD7000000   MAX 15B*/
    if((target_status & 0xFF000000) == CALRTC_START)    Fl2CalRTCEntry();
  }
}
