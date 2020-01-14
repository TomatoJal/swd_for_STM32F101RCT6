#include "swd_dap.h"
#include "swd_app.h"
#include "RTC.h"
#include "m25pxx.h"
#include "firmware.h"
#include "ict_communication.h"
#include "misc.h"
#include <string.h>

#define TIMING_GETINFO      10
#define TIMING_ERASE        50 
#define TIMING_PROGRAM      50
#define TIMING_VERIFY       50
#define TIMEOUT_GETINFO     10000
#define TIMEOUT_ERASE       50000
#define TIMEOUT_PROGRAM     50000
#define TIMEOUT_VERIFY      50000

#define REGISTWE_R(x)  x
#define xPSRVALUE      0xC1000000
#define ENTERADDR      0x20000004
#define MSPOFFSET      0x300

#define IMAGE_SIZE     0x400

#define MAXRETRYTIMES  5z
tTargetInfo TargetInfo;//�豸��Ϣ 

uint8  ImageArr[IMAGE_SIZE];//��flash���ó���1k����

static uint8 FailRetry = 0;

static uint8 Target_GetInfo(void);
static uint8 Target_BootLoader(void);
static uint8 Target_Erase(void);
static uint8 Target_Program(void);
static uint8 Target_Verify(void);
static uint8 Target_Reset(void);

/*****************************************************************************************
��������: Target_BootLoader
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
#define RAM_CHECK 1
static uint8 Target_RamCheck(void)
{
    uint32 i;
    uint32 val = 0;
    /*ֹͣ�ں�*/
    Check(SWD_CoreStop());
    Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
#if (RAM_CHECK == 1)    
    /*���RAM����*/
    for(uint32 i = 0 ; i < TargetInfo.bootloader_size/4 ; i++)
    {
        Check(SWD_WriteWord(0x20000000 + (i * 4), 0xFFFFFFFF));
        Check(SWD_ReadWord(0x20000000 + (i * 4), &val));
        if(val != 0xFFFFFFFF)
        {
            return RESULT_ERROR;
        }
        Check(SWD_WriteWord(0x20000000 + (i * 4), 0x00000000));
        Check(SWD_ReadWord(0x20000000 + (i * 4), &val));
        if(val != 0x000000)
        {
            return RESULT_ERROR;
        }
        Check(SWD_WriteWord(0x20000000 + (i * 4), 0xFFFFFFFF));
        Check(SWD_ReadWord(0x20000000 + (i * 4), &val));
        if(val != 0xFFFFFFFF)
        {
            return RESULT_ERROR;
        }
        Check(SWD_WriteWord(0x40010004, 0x0000AAFF));        
    }
#endif     
    return RESULT_OK;
}

/*****************************************************************************************
��������: Target_BootLoader
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 Target_BootLoader(void)
{
    uint32 i;
    uint32 val = 0;
    /*ֹͣ�ں�*/
    Check(SWD_CoreStop());
    /*����λ��������������ں˸�λ��ֹͣ����*/
    Check(SWD_WriteWord(DBG_EMCR, VC_CORERESET));
    /*����Ӧ���ж��븴λ���ƼĴ�����ʹϵͳ��λ����һ������Ҫ*/
    Check(SWD_WriteWord(NVIC_AIRCR, VECTKEY | VECTRESET));
    /*��λ�ں�*/
    Check(SWD_CoreReset()); 
    Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
    /*дbootloader*/
    for(i = 0 ; i < ((TargetInfo.bootloader_size / 1024)+1) ; i++)
    {
        if(i == (TargetInfo.bootloader_size / 1024))
        {
            M25_ReadBytes(ASSPROG_BASE_ADDRESS + (i * 1024), ImageArr,  TargetInfo.bootloader_size - i*1024);
            Check(SWD_WriteMemoryArray(TargetInfo.RAM_start + (i * 1024), 
                                       ImageArr, 
                                       TargetInfo.bootloader_size - i*1024 ));
        }
        else
        {
            M25_ReadBytes(ASSPROG_BASE_ADDRESS + (i * 1024), ImageArr,  1024);
            Check(SWD_WriteMemoryArray(TargetInfo.RAM_start + (i * 1024), 
                                       ImageArr, 
                                       1024 ));
        } 
        Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
    }
    /*����xPSRΪ0x01000000*/
    Check(SWD_WriteCoreRegister(xPSR , xPSRVALUE));
    //Check(SWD_ReadCoreRegister(xPSR , &val));
    /*����ִ�е���һ������ָ��ĵ�ַ��ָ��RAM��ַ*/
    Check(SWD_ReadWord(ENTERADDR,&TargetInfo.entry_addr)); 
    Check(SWD_WriteCoreRegister(REGISTWE_R(15) , TargetInfo.entry_addr));
    /*����ջָ�����boot�󣬱�����*/
    Check(SWD_WriteCoreRegister(MSP , TargetInfo.RAM_start + 0x3000));
    Check(SWD_CoreReset());  
    /*ʹ�ܵ���*/
    Check(SWD_WriteWord(DBG_HCSR, DBGKEY | C_DEBUGEN));
    MCUDelayMs(1000);
    Check(SWD_CoreStop());
    Check(SWD_ReadCoreRegister(REGISTWE_R(15) , &val));
    Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
    if((val < 0x20000000)||(val > 0x20010000))
    {
        FailRetry++;
    }
    else
    {
        Check(SWD_CoreStart());
    }
    return RESULT_OK;
}
/*****************************************************************************************
��������: Target_GetInfo
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 Target_GetInfo(void)
{  
    uint32 tmptime = SysTime;   
    
    while((SysTime-tmptime) < TIMEOUT_GETINFO)
    {
        if((SysTime-tmptime) % TIMING_GETINFO == 0)
        {  
            Check(SWD_ReadWord(TargetInfo.status_addr,&TargetInfo.status));
            if((TargetInfo.status & 0xFF000000) == TARGET_RAMGOT)
            {
                TargetInfo.RAM_size = (TargetInfo.status & 0x00FFFFFF);
                return RESULT_OK;
            }
            Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
        }
    }
    /*��ʱ*/
    return RESULT_TIMEOUT; 
}
/*****************************************************************************************
��������: Target_Erase
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 Target_Erase(void)
{
    uint32 tmptime = 0;
    /*д�����ʼָ��*/
    MCUDelayMs(100);
    Check(SWD_WriteWord(TargetInfo.status_addr, ERASE_START+TargetInfo.bootloader_size)); 
    MCUDelayMs(100);
    tmptime = SysTime;
    while((SysTime-tmptime) < TIMEOUT_ERASE)
    {
        if((SysTime-tmptime) % TIMING_ERASE == 0)
        {     
            Check(SWD_ReadWord(TargetInfo.status_addr,&TargetInfo.status));  
            /*SUCCESS*/
            if(TargetInfo.status == ERASE_OK)
            {
                Check(SWD_WriteWord(TargetInfo.status_addr, ERASE_END));  
                return RESULT_OK;;
            }
            /*FAIL*/
            else if(TargetInfo.status == ERASE_ERROR)
            {
                Check(SWD_WriteWord(TargetInfo.status_addr, ERASE_END));
                return RESULT_ERROR;
            }
            Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
        }
    }
    /*��ʱ*/
    return RESULT_TIMEOUT;
}
/*****************************************************************************************
��������: Target_Program
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 Target_Program(void)
{
    uint32 i;
    uint32 val = 0;
    uint32 tmptime = 0;
    /*����*/
    val = ICT_GetImageLength(METER_IMAGE) % (TargetInfo.RAM_size / 2);
    if(val == 0)
    {
        TargetInfo.status 
            = ICT_GetImageLength(METER_IMAGE) / (TargetInfo.RAM_size / 2); 
    }
    else//���һ��δ��  
    {
        TargetInfo.status 
            = (ICT_GetImageLength(METER_IMAGE) / (TargetInfo.RAM_size / 2)) + 1;
    }
    TargetInfo.image_num = (uint8)TargetInfo.status;

    Check(SWD_WriteWord(TargetInfo.status_addr,PROGRAM_START + (uint16)(TargetInfo.status << 8))); 
    tmptime = SysTime;
    /*�ȴ�����ͬ��*/
    while((SysTime-tmptime) < TIMEOUT_GETINFO)
    {
        if((SysTime-tmptime) % TIMING_GETINFO == 0)
        {  
            Check(SWD_ReadWord(TargetInfo.status_addr,&TargetInfo.status));
            if((uint8)TargetInfo.status == (uint8)(TargetInfo.status >> 8))
            {
                break;
            }
            Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
        }
    }
    /*��ʱ*/
    if((SysTime-tmptime) >= TIMEOUT_GETINFO)
    {
        return RESULT_TIMEOUT;
    }
    /*��ʼд��*/
    tmptime = SysTime;
    while((SysTime-tmptime) < TIMEOUT_PROGRAM)
    {
        if((SysTime-tmptime) % TIMING_PROGRAM == 0)
        {
            /*��д��*/
            if(((uint8)TargetInfo.status == (uint8)(TargetInfo.status >> 8)) && 
               ((uint16)TargetInfo.status != 0))
            {
                for(i = 0 ; i < ((TargetInfo.RAM_size) / 2048) ; i++)
                {
                    M25_ReadBytes(MTRPROG_BASE_ADDRESS + 
                                 (TargetInfo.image_num - (uint8)TargetInfo.status) * ((TargetInfo.RAM_size) / 2) + (i * 1024), 
                                  ImageArr,  1024);
                    Check(SWD_WriteMemoryArray(TargetInfo.RAM_start + (TargetInfo.RAM_size / 2) + (i * 1024), ImageArr, 1024 ));
                }
                TargetInfo.status -= 0x100; 
                Check(SWD_WriteWord(TargetInfo.status_addr,TargetInfo.status));    
            }
            Check(SWD_ReadWord(TargetInfo.status_addr,&TargetInfo.status)); 
            /*SUCCESS*/
            if(TargetInfo.status == PROGRAM_OK) 
            {
                Check(SWD_WriteWord(TargetInfo.status_addr,PROGRAM_END));  
                return RESULT_OK;
            }
            /*FAIL*/
            else if(TargetInfo.status == PROGRAM_ERROR) 
            {
                Check(SWD_WriteWord(TargetInfo.status_addr,PROGRAM_END)); 
                return RESULT_ERROR;
            }
            Check(SWD_WriteWord(0x40010004, 0x0000AAFF));
        }
    }
    return RESULT_TIMEOUT;
}
/*****************************************************************************************
��������: Target_Verify
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 Target_Verify(void)
{
    uint8  zero[2] = { 0, 0 };
    uint32 tmptime = 0;
    uint16 Checksum = 0;//crcУ���
    M25_ReadBytes(MTRIMAGE_BASE_ADDRESS + CRC_ADDRESS , zero , 2);
    Checksum = (uint16)(zero[1] << 8) + (uint16)zero[0];
    /*д��У�鿪ʼ + image��*/  
    Check(SWD_WriteWord(TargetInfo.status_addr,VERIFY_START + ICT_GetImageLength(METER_IMAGE)));
    tmptime = SysTime;
    while((SysTime-tmptime) < TIMEOUT_VERIFY)
    {
        if((SysTime-tmptime) % TIMING_VERIFY == 0)
        {
            Check(SWD_ReadWord(TargetInfo.status_addr,&TargetInfo.status));
            if((TargetInfo.status & 0xFF000000)== VERIFY_OK)
            {
                Check(SWD_WriteWord(TargetInfo.status_addr,VERIFY_END));
                if((uint16)TargetInfo.status == Checksum)
                {
                    return  RESULT_OK;
                }
                else
                {
                    return  RESULT_ERROR;
                }
            } 
            Check(SWD_WriteWord(0x40010004, 0x0000AAFF));

        }
    }
    return RESULT_TIMEOUT;
}

/*****************************************************************************************
��������: Target_FailRetry
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 Target_FailRetry(void)
{
    if(FailRetry)
    {
//        if(FailRetry >= MAXRETRYTIMES)
//        {
//            FailRetry = 0;
//            return RESULT_ERROR;
//        }
        FailRetry = 0;
        Check(SWD_Init());
        Check(Target_BootLoader()); 
        if(FailRetry)
        {
            FailRetry = 0;
            return RESULT_ERROR;
        }  
    }
    return RESULT_OK;
}
/*****************************************************************************************
��������: Target_Reset
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
#define SCB_AIRCR_VECTKEY_Pos              16                                             /*!< SCB AIRCR: VECTKEY Position */
#define SCB_AIRCR_VECTKEY_Msk              (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)            /*!< SCB AIRCR: VECTKEY Mask */
#define SCB_AIRCR_SYSRESETREQ_Pos           2                                             /*!< SCB AIRCR: SYSRESETREQ Position */
#define SCB_AIRCR_SYSRESETREQ_Msk          (1UL << SCB_AIRCR_SYSRESETREQ_Pos)             /*!< SCB AIRCR: SYSRESETREQ Mask */
static uint8 Target_Reset(void)
{
  if(SWD_WriteWord(NVIC_AIRCR, ((0x5FAUL << SCB_AIRCR_VECTKEY_Pos)|SCB_AIRCR_SYSRESETREQ_Msk)))  return  RESULT_ERROR;
  MCUDelayMs(10);
  Check(SWD_CoreReset());
  Check(SWD_CoreStart()); 
  MCUDelayMs(990);
  return RESULT_OK;
}

/*****************************************************************************************
��������: Target_API_InfoInit
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
uint8 Target_API_InfoInit(void)
{
    TargetInfo.RAM_start = 0x20000000;//�̶�    
    TargetInfo.RAM_size  = 0;//δ֪
    TargetInfo.bootloader_size = ICT_GetImageLength(ASSIST_IMAGE);//��֪
    TargetInfo.status_addr = 0x20000020;//�̶�
    TargetInfo.status = 0;//��ʼ��
    return 0;
}
/*****************************************************************************************
��������: Target_API_Erase
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
uint8 Target_API_Erase(void)
{
  
    if(SWD_Init())          return DOWNLOAD_ERROR_INIT;
    if(Target_RamCheck())   return DOWNLOAD_ERROR_INIT;
    if(Target_BootLoader()) return DOWNLOAD_ERROR_BOOT;
    if(Target_FailRetry())  return DOWNLOAD_ERROR_UNKNOW;
    if(Target_GetInfo())    return DOWNLOAD_ERROR_GETINFO;
    if(Target_Erase())      return DOWNLOAD_ERROR_ERASE;
    return DOWNLOAD_DONE;
}
/*****************************************************************************************
��������: Target_API_PROGRAM
��ڲ���: ��
���ڲ���: ��
���ز�������  
�������ܣ���
*****************************************************************************************/
uint8 Target_API_PROGRAM(void)
{
    if(SWD_Init())          return DOWNLOAD_ERROR_INIT;
    if(Target_RamCheck())   return DOWNLOAD_ERROR_INIT;
    if(Target_BootLoader()) return DOWNLOAD_ERROR_BOOT;
    if(Target_FailRetry())  return DOWNLOAD_ERROR_UNKNOW;
    if(Target_GetInfo())    return DOWNLOAD_ERROR_GETINFO;
    if(Target_Erase())      return DOWNLOAD_ERROR_ERASE;
    if(Target_Program())    return DOWNLOAD_ERROR_PROGRAM; 
    if(Target_Reset())      return DOWNLOAD_ERROR_RESET;
    return DOWNLOAD_DONE;
}
/*****************************************************************************************
��������: Target_API_PROGRAM
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
uint8 Target_API_VERIFY(void)
{
    if(SWD_Init())          return DOWNLOAD_ERROR_INIT;
    if(Target_BootLoader()) return DOWNLOAD_ERROR_BOOT;
    if(Target_FailRetry())  return DOWNLOAD_ERROR_UNKNOW;
    if(Target_GetInfo())    return DOWNLOAD_ERROR_GETINFO;
    if(Target_Verify())     return DOWNLOAD_ERROR_VERIFY;
    return DOWNLOAD_DONE;
}
/*****************************************************************************************
��������: Target_API_AUTO
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
uint8 Target_API_AUTO(void)
{
//    if(SWD_Init())          return DOWNLOAD_ERROR_INIT;     //1348  4
//    if(Target_BootLoader()) return DOWNLOAD_ERROR_BOOT;     //1352  6367
//    if(Target_FailRetry())  return DOWNLOAD_ERROR_UNKNOW;   //7719  2
//    if(Target_GetInfo())    return DOWNLOAD_ERROR_GETINFO;  //7721  3
//    if(Target_Erase())      return DOWNLOAD_ERROR_ERASE;    //7724  191
//    //if(CalRTC_Process())    return CalRTC_ERROR;
//    if(Target_Program())    return DOWNLOAD_ERROR_PROGRAM;  //7915  14303
//    if(Target_Verify())     return DOWNLOAD_ERROR_VERIFY;   //22218 1553
//    if(Target_Reset())      return DOWNLOAD_ERROR_RESET;    //23771 447
    
    if(SWD_Init())          return DOWNLOAD_ERROR_INIT;
    if(Target_RamCheck())   return DOWNLOAD_ERROR_INIT;
    if(Target_BootLoader()) return DOWNLOAD_ERROR_BOOT;
    if(Target_FailRetry())  return DOWNLOAD_ERROR_UNKNOW;
    if(Target_GetInfo())    return DOWNLOAD_ERROR_GETINFO;
    if(Target_Erase())      return DOWNLOAD_ERROR_ERASE;
    if(Target_Program())    return DOWNLOAD_ERROR_PROGRAM; 
    if(Target_Verify())     return DOWNLOAD_ERROR_VERIFY;  
    if(CalRTC_Process())    return CalRTC_ERROR;
    if(Target_Reset())      return DOWNLOAD_ERROR_RESET;
    
#if (0)
    if(SWD_Init())            return CalRTC_ERROR_CHECK;
    if(CalRTC_ProcessCheck()) return CalRTC_ERROR_CHECK;
    if(Target_Reset())        return CalRTC_ERROR_CHECK;
#endif
    return DOWNLOAD_DONE; 
}
/*****************************************************************************************
��������: SWD_DataInit
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
uint16 Crc16_helper(uint8 const *p, uint32 len, uint16 sum)
{
  while (len--)
  {
    int i;
    uint8 byte = *p++;

    for (i = 0; i < 8; ++i)
    {
      uint32 osum = sum;
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