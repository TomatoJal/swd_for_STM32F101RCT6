#include "ht6xxx_RTC.h"
#include "ht6xxx_flash.h"
#include "ht6xxx_cmu.h"

__root __no_init extern uint32_t target_status           @0x20001000;

int16_t  Toff; uint16_t Toff_Temp;
int16_t  Toff_Code;
uint16_t TAB_DFx[10];
uint16_t MCONxx[3];

static int16_t HT_RTC_TemperatureGet();
static uint32_t HT_RTC_Shift(uint8_t member);

static void Wait200mS(void)
{
    
    uint32_t delay;
    
    delay = HT_CMU_SysClkGet();                             /*!< 获取当前系统时钟Fsys     */
    delay = delay>>(HT_CMU->SYSCLKDIV & CMU_SYSCLKDIV );    /*!< 获取当前CPU时钟Fcpu      */
    delay *= 3000;
    delay = delay>>10;                                      /*!< 500 X 2                  */
    
    while((delay--) != 0){;}                                       /*!< 等待2ms                  */       
}

static int16_t HT_RTC_TemperatureGet()
{
    int16_t temp[TEMP_GET_TIMES];
    int16_t temp_max=0,temp_min=0;
    int32_t temp_sum=0;
    uint8_t  i;
    for(i = 0; i < TEMP_GET_TIMES; i++)
    {
        temp[i] = HT_TBS->TMPDAT;
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
            else
            {
                temp_min = temp[i];
            }
        }
        Wait200mS();
    }
 
    return (temp_sum - temp_max - temp_min)/(TEMP_GET_TIMES - 2);
}

/*0~4*/
static uint32_t HT_RTC_Shift(uint8_t member)
{
    uint32_t DFx;
    DFx = (uint32_t)(((uint32_t)TAB_DFx[member*2] <<16) + TAB_DFx[member*2 + 1]);
    if(DFx&0x400000) DFx |=0xFF80000;
    return DFx;
}

/*2.检查Toff值是否合法*/
void HT_RTC_ToffCheck()
{
    uint32_t val = 0;
    Toff = (int16_t)(*(uint32_t *)ADDRESS_TOFF);//Toff值      范围-3000~3000
    uint16_t toff_temp = (uint16_t)((*(uint32_t *)ADDRESS_CODE)>>16);//温度        范围20~30
    int16_t  toff_code = (int16_t)(*(uint32_t *)ADDRESS_CODE);//TMPDAT值  范围-7000~-1000
    if((Toff > 3000) ||(Toff < -3000)||
       (toff_temp > 3000) ||(toff_temp < 2000)||  
       (toff_code > -1000)||(toff_code < -7000))
    {
        HT_CMU->INFOLOCK_A = CMU_INFOLOCK_UnLocked;
        HT_Flash_WordWrite(&val,ADDRESS_TOFF, 1);
        HT_CMU->INFOLOCK_A = CMU_INFOLOCK_Locked;
        HT_RTC->Toff = DEFAULT_TOFF;
    }
    else
    {
        HT_RTC->Toff = Toff;	
    }
}
/*3. 检查RTC系数*/
void HT_RTC_RTCInfoCheck()
{
    uint8_t i;

    uint32_t checksumtmp = 0;
    uint32_t checksum = *(uint32_t *)ADDRESS_CHECKSUM;
    for(i = 0; i < sizeof(TAB_DFx)/sizeof(TAB_DFx[0]); i++)
    {
        TAB_DFx[i] = (uint16_t)(*(uint32_t *)(ADDRESS_DFAH + i*4));
        checksumtmp += TAB_DFx[i];
    }
    for(i = 0; i < sizeof(MCONxx)/sizeof(MCONxx[0]); i++)
    {
        MCONxx[i] = (uint16_t)(*(uint32_t *)(ADDRESS_MCON01 + i*4));
        checksumtmp += MCONxx[i];
    }
    checksumtmp += (uint16_t)Toff;
    if(checksumtmp == checksum)
    {
        HT_RTC->DFAH   = TAB_DFx[0];
        HT_RTC->DFAL   = TAB_DFx[1];    
        HT_RTC->DFBH   = TAB_DFx[2];
        HT_RTC->DFBL   = TAB_DFx[3];
        HT_RTC->DFCH   = TAB_DFx[4];
        HT_RTC->DFCL   = TAB_DFx[5];
        HT_RTC->DFDH   = TAB_DFx[6];
        HT_RTC->DFDL   = TAB_DFx[7];
        HT_RTC->DFEH   = TAB_DFx[8];
        HT_RTC->DFEL   = TAB_DFx[9];
        HT_RTC->MCON01 = MCONxx[0];	
        HT_RTC->MCON23 = MCONxx[1];	
        HT_RTC->MCON45 = MCONxx[2];
    }
    else
    {
        HT_RTC->DFAH   = DEFAULT_DFAH;	
        HT_RTC->DFAL   = DEFAULT_DFAL;	    
        HT_RTC->DFBH   = DEFAULT_DFBH;	
        HT_RTC->DFBL   = DEFAULT_DFBL;	
        HT_RTC->DFCH   = DEFAULT_DFCH;	
        HT_RTC->DFCL   = DEFAULT_DFCL;	
        HT_RTC->DFDH   = DEFAULT_DFDH;	
        HT_RTC->DFDL   = DEFAULT_DFDL;	
        HT_RTC->DFEH   = DEFAULT_DFEH;	
        HT_RTC->DFEL   = DEFAULT_DFEL;
        HT_RTC->MCON01 = DEFAULT_MCON01;	
        HT_RTC->MCON23 = DEFAULT_MCON23;	
        HT_RTC->MCON45 = DEFAULT_MCON45;	
        TAB_DFx[0]     = DEFAULT_DFAH;
        TAB_DFx[1]     = DEFAULT_DFAL;
        TAB_DFx[2]     = DEFAULT_DFBH;
        TAB_DFx[3]     = DEFAULT_DFBL;
        TAB_DFx[4]     = DEFAULT_DFCH;
        TAB_DFx[5]     = DEFAULT_DFCL;
        TAB_DFx[6]     = DEFAULT_DFDH;
        TAB_DFx[7]     = DEFAULT_DFDL;
        TAB_DFx[8]     = DEFAULT_DFEH;
        TAB_DFx[9]     = DEFAULT_DFEL;
    }
}
/*4. 计算dFi*/
static int32_t HT_RTC_CaldFi()
{    
    int16_t temp;
    uint32_t dFi;
    int32_t Ra,Rb,Rc,Rd,Re;
    int16_t Tc = 0;
    temp =HT_RTC_TemperatureGet();
    Tc = temp-Toff;;
    Ra = HT_RTC_Shift(0);
    Rb = HT_RTC_Shift(1);
    Rc = HT_RTC_Shift(2);
    Rd = HT_RTC_Shift(3);
    Re = HT_RTC_Shift(4);
    
    dFi = (Re  * Tc >>16) + Rd;
    dFi = (dFi * Tc >>16) + Rc;
    dFi = (dFi * Tc >>16) + Rb;
    dFi = (dFi * Tc >>16) + Ra;
    dFi = (dFi + 0x02)>>2;
    dFi=(
         (
          (
           (
            (
             (
              (
               (
            (Re*Tc>>16)+Rd
            )*Tc>>16)+Rc 
           )*Tc>>16)+Rb 
          )*Tc>>16)+ Ra
         )+0x02)>>2;
    dFi = Ra +
          Rb * Tc + 
          Rc * Tc * Tc + 
          Rd * Tc * Tc * Tc +
          Re * Tc * Tc * Tc * Tc;  
    return dFi;
}

void HT_RTC_Calerr()
{
    int32_t dFi = HT_RTC_CaldFi();
    double err = (double)(target_status&0xFFFF)/1000000;
}