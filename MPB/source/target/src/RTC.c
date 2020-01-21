#include "RTC.h"
#include "measure_freq.h" 
#include "firmware.h"
#include "swd_app.h"
#include "swd_dap.h"
#include "misc.h"
#include "string.h"

#define  CETUS02

#define  STANDARD_FREQ                           (double)1.000000
#define  STANDARD_SYNALLOWEDERR                  (double)0.000347//30s/d 1hz 0.000347Hz
#define  STANDARD_REALALLOWEDERR                 (double)0.000003//3ppm
#define  TEMP_GET_TIMES  20
#define  TEMP_GET_TIMING 500

#define CALRTC_START                              0xD7000000
#define CALRTC_END                                0x7D000000                    
#define CALRTC_ERROR                              0x77000000
#define CALRTC_OK                                 0xDD000000
#define TIMING_CALRTC                             50
#define TIMEOUT_CALRTC                            10000

#define  RTC_RTCCON_TOUT                         ((uint32_t)0x001E)           /*!<Tout输出控制          */
#define  RTC_RTCCON_TOUT_LOW                     ((uint32_t)0x0000)           /*!<Tout输出低            */
#define  RTC_RTCCON_TOUT_HIGH                    ((uint32_t)0x0002)           /*!<Tout输出高            */
#define  RTC_RTCCON_TOUT_LF                      ((uint32_t)0x0004)           /*!<Tout输出低频时钟      */
#define  RTC_RTCCON_TOUT_1HZ                     ((uint32_t)0x0006)           /*!<Tout输出高频补偿1hz   */
#define  RTC_RTCCON_TOUT_2HZ                     ((uint32_t)0x0008)           /*!<Tout输出高频补偿2hz   */
#define  RTC_RTCCON_TOUT_4HZ                     ((uint32_t)0x000A)           /*!<Tout输出高频补偿4hz   */
#define  RTC_RTCCON_TOUT_8HZ                     ((uint32_t)0x000C)           /*!<Tout输出高频补偿8hz   */
#define  RTC_RTCCON_TOUT_16HZ                    ((uint32_t)0x000E)           /*!<Tout输出高频补偿16hz  */
#define  RTC_RTCCON_TOUT_32HZ                    ((uint32_t)0x0010)           /*!<Tout输出高频补偿32hz  */
#define  RTC_RTCCON_TOUT_64HZ                    ((uint32_t)0x0012)           /*!<Tout输出高频补偿64hz  */
#define  RTC_RTCCON_TOUT_128HZ                   ((uint32_t)0x0014)           /*!<Tout输出高频补偿128hz */
typedef enum
{ 
    ToutLOW    = RTC_RTCCON_TOUT_LOW, 
    ToutHigh   = RTC_RTCCON_TOUT_HIGH,  
    ToutLF     = RTC_RTCCON_TOUT_LF,    
    Tout1Hz    = RTC_RTCCON_TOUT_1HZ,   
    Tout2Hz    = RTC_RTCCON_TOUT_2HZ,
    Tout4Hz    = RTC_RTCCON_TOUT_4HZ,
    Tout8Hz    = RTC_RTCCON_TOUT_8HZ,
    Tout16Hz   = RTC_RTCCON_TOUT_16HZ,
    Tout32Hz   = RTC_RTCCON_TOUT_32HZ,
    Tout64Hz   = RTC_RTCCON_TOUT_64HZ,
    Tout128Hz  = RTC_RTCCON_TOUT_128HZ,
}RTCTout_TypeDef;                  /*!< end of group RTCTout_TypeDef  */  

const uint16 TAB_DFxDefault[10]=
{
#ifdef CETUS02
    0x0000,0x0000,
    0x007F,0xE55F,
    0x007E,0xD398,
    0x0000,0x4033,
    0x007F,0xF21D
#else
    0x0000,0x0000,  //DFA                     //16进制
    0x007F,0xDB61,  //DFB
    0x007E,0xD8AC,  //DFC
    0x0000,0x4ACF,  //DFD
    0x007F,0xFA88   //DFE
#endif
};

static int32 TAB_Rx[5];
static int16 sgTemperature;
static int32   DFi;
static uint16 TAB_DFx[10];
static uint16 MCONxx[3];
#define  INFOBLOCKSIZE                           1024
static uint8 InfoBlockBack[INFOBLOCKSIZE];
static uint8 CalRTC_ToutSet(RTCTout_TypeDef RTCToutSet);
static uint8 CalRTC_Init(RTCTout_TypeDef RTCToutSet);
static uint8 CalRTC_GetFreqErr();

static uint8 CalRTC_ToutSet(RTCTout_TypeDef RTCToutSet)
{
    uint32 val = 0;
    
    Check(SWD_ReadWord(TBS_TBSCON_ADDRESS,&val));
    Check(SWD_WriteWord(TBS_TBSCON_ADDRESS,val|0x6541));
    
    Check(SWD_ReadWord(GPIOC_IOCFG_ADDRESS,&val));  
    Check(SWD_WriteWord(GPIOC_IOCFG_ADDRESS,val|(1UL << 8)));
    
    Check(SWD_ReadWord(GPIOC_AFCFG_ADDRESS,&val));
    Check(SWD_WriteWord(GPIOC_AFCFG_ADDRESS,(val&0xFFFFFEFF)));
    
    Check(SWD_ReadWord(GPIOC_PTUP_ADDRESS,&val));
    Check(SWD_WriteWord(GPIOC_PTUP_ADDRESS,val|(1UL << 8)));
    
    Check(SWD_ReadWord(GPIOC_PTOD_ADDRESS,&val));
    Check(SWD_WriteWord(GPIOC_PTOD_ADDRESS,val|(1UL << 8)));
    
    Check(SWD_WriteWord(RTC_RTCCON_ADDRESS,RTC_RTCCON_TOUT_1HZ)); 
    return RESULT_OK; 
}

static uint8 CalRTC_Init(RTCTout_TypeDef RTCToutSet)
{
//    SWD_Init();
    
    Check(CalRTC_ToutSet(RTCToutSet));
    return RESULT_OK; 
}

static uint8 CalRTC_InfoBlockGet(uint8* buf)
{
    Check(SWD_ReadMemoryArray(INFOBLOCK_ADDRESS,buf,INFOBLOCKSIZE));
    return RESULT_OK; 
}


static uint8 CalRTC_GetFreqErr()
{ 
    DMA_Reconfig();
    Check(CheckMeasureResult());
    return RESULT_OK;  
}

static uint8 CalRTC_IsSynAllowed()
{ 
    if(((STANDARD_FREQ + STANDARD_SYNALLOWEDERR)<TargetInfo.frequency) ||
    ((TargetInfo.frequency + STANDARD_SYNALLOWEDERR)<STANDARD_FREQ))
    {
        return RESULT_ERROR;   
    }   
    return RESULT_OK;  
}
static uint8 CalRTC_RTCInfoSyn(void)
{
    uint32 tmptime = 0;
    uint32 val;
    /*写入擦开始指令*/
    Check(SWD_WriteWord(TargetInfo.status_addr, CALRTC_START)); 
    MCUDelayMs(200);
    tmptime = SysTime;
    while((SysTime-tmptime) < TIMEOUT_CALRTC)
    {
        if((SysTime-tmptime) % TIMING_CALRTC == 0)
        {     
            Check(SWD_ReadWord(TargetInfo.status_addr,&TargetInfo.status));  
            Check(SWD_CoreStop());
            Check(SWD_ReadCoreRegister(REGISTWE_R(15) , &val));
            Check(SWD_CoreStart());
            /*SUCCESS*/
            if(TargetInfo.status == CALRTC_OK)
            {
                Check(SWD_WriteWord(TargetInfo.status_addr, CALRTC_END));  
                return RESULT_OK;;
            }
            /*FAIL*/
            else if(TargetInfo.status == CALRTC_OK)
            {
                Check(SWD_WriteWord(TargetInfo.status_addr, CALRTC_END));
                return RESULT_ERROR;
            }
        }
    }
    /*超时*/
    return RESULT_TIMEOUT;
}

static uint8 CalRTC_RTCInfoSave()
{ 
    double Fn = 0;
    double Err = 0;
    uint8  i  ;
    uint32 TAB_Rxsave[5];
    int32 DFA = 0;
    int16  toff = ARR2U32(&InfoBlockBack[TOFF_OFFSET]);
    
    Fn = 32768+(double)DFi/512;
    Err = TargetInfo.frequency*86400-86400;
    Fn = Fn*Err/(86400- Err);
    DFA = (int32)(Fn*128*16);
    TAB_Rx[0] += DFA;
    memcpy(TAB_Rxsave, TAB_Rx, 20);
    for(i = 0; i < 5; i++)
    {
        if((TAB_Rxsave[i]&0xFF800000) == 0xFF800000)
        {
            TAB_Rxsave[i] = (TAB_Rxsave[i]&(~0xFF800000));
        }
        Check(SWD_WriteWord(TargetInfo.RAM_start+TargetInfo.RAM_size/2+i*4,TAB_Rxsave[i]));
    }
    Check(SWD_WriteWord(TargetInfo.RAM_start+TargetInfo.RAM_size/2+i*4,(uint32)toff));
    for(i = 0; i < 3; i++)
    {
         Check(SWD_WriteWord(TargetInfo.RAM_start+TargetInfo.RAM_size/2+6*4+i*4,MCONxx[i]));
    }
    
    Check(CalRTC_RTCInfoSyn()); 
    return RESULT_OK;  
}

static uint8 CalRTC_ToffCheck()
{
    int16  toff = ARR2U32(&InfoBlockBack[TOFF_OFFSET]);
    uint16 toff_temp = ARR2U32(&InfoBlockBack[TEMP_OFFSET]);
    int16  toff_code = ARR2U32(&InfoBlockBack[CODE_OFFSET]);
    if((toff > 3000) ||(toff < -3000)||
       (toff_temp > 3000) ||(toff_temp < 2000)||  
       (toff_code > -1000)||(toff_code < -7000))
    {
//        Check(SWD_WriteHalfWord(CMU_INFOLOCK_A_ADDRESS,CMU_INFOLOCK_UnLocked));
//        Check(SWD_WriteHalfWord(INFOBLOCK_ADDRESS+TOFF_OFFSET,0));
//        Check(SWD_WriteHalfWord(CMU_INFOLOCK_A_ADDRESS,CMU_INFOLOCK_Locked));
        Check(SWD_WriteHalfWord(RTC_TOFF_ADDRESS,0));      
    }
    else
    {
        Check(SWD_WriteHalfWord(RTC_TOFF_ADDRESS,toff));	
    }
    return RESULT_OK;  
}  

static uint8 CalRTC_RTCInfoCheck()
{
    uint8  i;

    uint32 checksumtmp = 0;
    uint32 checksum = ARR2U32(&InfoBlockBack[CHECKSUM_OFFSET]);
    
    for(i=0; i<sizeof(TAB_DFx)/sizeof(TAB_DFx[0]); i++)
    {
        TAB_DFx[i] = ARR2U16(&InfoBlockBack[DFAH_OFFSET + i*4]);
        checksumtmp += TAB_DFx[i];
    }
    for(i=0; i<sizeof(MCONxx)/sizeof(MCONxx[0]); i++)
    {
        MCONxx[i] = ARR2U16(&InfoBlockBack[MCON01_OFFSET + i*4]);
        checksumtmp += MCONxx[i];
    }
    checksumtmp += ARR2U32(&InfoBlockBack[TOFF_OFFSET]);
    if(checksumtmp != checksum)
    {
        memcpy(TAB_DFx,TAB_DFxDefault,sizeof(TAB_DFx));
    }
#ifdef CETUS02
        //memcpy(TAB_DFx+2,TAB_DFxDefault+2,sizeof(TAB_DFx)-2);
        //将原DFA标0重新校准
        memcpy(TAB_DFx,TAB_DFxDefault,sizeof(TAB_DFx));
#endif
    /*写进寄存器跑来测误差*/
    Check(SWD_WriteHalfWord(RTC_DFAH_ADDRESS,TAB_DFx[0]));
    Check(SWD_WriteHalfWord(RTC_DFAL_ADDRESS,TAB_DFx[1]));
    Check(SWD_WriteHalfWord(RTC_DFBH_ADDRESS,TAB_DFx[2]));
    Check(SWD_WriteHalfWord(RTC_DFBL_ADDRESS,TAB_DFx[3]));
    Check(SWD_WriteHalfWord(RTC_DFCH_ADDRESS,TAB_DFx[4]));
    Check(SWD_WriteHalfWord(RTC_DFCL_ADDRESS,TAB_DFx[5]));
    Check(SWD_WriteHalfWord(RTC_DFDH_ADDRESS,TAB_DFx[6]));
    Check(SWD_WriteHalfWord(RTC_DFDL_ADDRESS,TAB_DFx[7]));
    Check(SWD_WriteHalfWord(RTC_DFEH_ADDRESS,TAB_DFx[8]));
    Check(SWD_WriteHalfWord(RTC_DFEL_ADDRESS,TAB_DFx[9]));
    Check(SWD_WriteHalfWord(RTC_MCON01_ADDRESS,MCONxx[0]));
    Check(SWD_WriteHalfWord(RTC_MCON23_ADDRESS,MCONxx[1]));
    Check(SWD_WriteHalfWord(RTC_MCON45_ADDRESS,MCONxx[2]));
    
    for(i = 0; i < sizeof(TAB_Rx)/sizeof(TAB_Rx[0]); i++)
    {
        TAB_Rx[i] = (TAB_DFx[i*2]<<16)|TAB_DFx[i*2 + 1];
        if (TAB_Rx[i] & 0x00400000)                                                                  //RTC系数为23位有符号数，需转换
        {
            TAB_Rx[i] |= 0xFF800000;
        }
    }
     return RESULT_OK; 
}


static uint8 CalRTC_TemperatureGet(uint16* temperature)
{
    int16  temp[TEMP_GET_TIMES];
    uint32 tempu16;
    int16 temp_max=0,temp_min=0;
    int32 temp_sum=0;
    uint8  i;
    for(i = 0; i < TEMP_GET_TIMES; i++)
    {
        Check(SWD_ReadHalfWord(TBS_TMPDAT_ADDRESS,&tempu16));
        temp[i] = tempu16;
        temp_sum += temp[i];
        if(i==0)
        {
            temp_max = temp[i];
            temp_min = temp[i];
        }
        else
        {
            if(temp[i] > temp_max)
            {
                temp_max = temp[i];
            }
            else if(temp[i] < temp_min)
            {
                temp_min = temp[i];
            }
        }
        MCUDelayMs(TEMP_GET_TIMING);
    }
 
    *temperature = (temp_sum - temp_max - temp_min)/(TEMP_GET_TIMES - 2);
    return RESULT_OK;
}


static uint8 CalRTC_HT_RTC_CaldFi()
{
    int16  toff = ARR2U32(&InfoBlockBack[TOFF_OFFSET]);
    int16  Tc = sgTemperature - toff;

     
    DFi  =  TAB_Rx[4];
    DFi *=  Tc;        //当前温度 Tc = TMPDAT-TOFF
    DFi >>= 16;

    DFi +=  TAB_Rx[3];
    DFi *=  Tc;
    DFi >>= 16;
    
    DFi +=  TAB_Rx[2];
    DFi *=  Tc;
    DFi >>= 16;

    DFi +=  TAB_Rx[1];
    DFi *=  Tc;
    DFi >>= 16;

    DFi +=  TAB_Rx[0];
    DFi +=  2;
    DFi >>= 2;
    
    Check(SWD_WriteHalfWord(RTC_DFIH_ADDRESS,DFi >>16));
    Check(SWD_WriteHalfWord(RTC_DFIL_ADDRESS,(int16)DFi));
    return RESULT_OK;        
} 



uint8 CalRTC_Process()
{
//    if(CalRTC_ProcessCheck())
//    {
        Check(CalRTC_Init(RTC_RTCCON_TOUT_1HZ))     ;//return//0x0A;
        Check(CalRTC_InfoBlockGet(InfoBlockBack))   ;//return//0x0A;
        Check(CalRTC_ToffCheck())                   ;//return//0x0A;
        Check(CalRTC_RTCInfoCheck())                ;//return//0x0A;
        Check(CalRTC_TemperatureGet(&sgTemperature));//return//0x0B;
        Check(CalRTC_HT_RTC_CaldFi())               ;//return//0x0C; 
        Check(CalRTC_GetFreqErr())                  ;//return//0x0D;
        //while(1);
        Check(CalRTC_IsSynAllowed())                ;//return//0x0E;
        Check(CalRTC_RTCInfoSave())                 ;//return//0x0F;
//    }
    MCUDelayMs(100);
    return RESULT_OK; 
}

uint8 CalRTC_ProcessCheck()
{
    Check(CalRTC_Init(RTC_RTCCON_TOUT_1HZ));
    Check(CalRTC_GetFreqErr());
    
    if(((STANDARD_FREQ + STANDARD_REALALLOWEDERR)<TargetInfo.frequency) ||
    ((TargetInfo.frequency + STANDARD_REALALLOWEDERR)<STANDARD_FREQ))
    {
        return RESULT_ERROR;   
    }  
    
    return RESULT_OK; 
}