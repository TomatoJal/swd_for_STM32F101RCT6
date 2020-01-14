#include "swj_dp.h"
#include "ict_communication.h"
#include "m25pxx.h"
#include "misc.h"

#define MAX_SWD_RETRY   1
#define TARGET_AUTO_INCREMENT_PAGE_SIZE    (0x400)

tSWJ_data SWJ_data;

/*******************************************************************************
************************************SWD���*************************************
*******************************************************************************/
void SWJ_DataInit(void)
{
  SWJ_data.clock_delay = 1;//ʱ�����ڳ�
}

u8 SWJ_Reset(void)
{
    u32 val = 0;
    SWDIO_SET();
    /*��ʼ53��������һ��0xE79E���и�λ*/ 
    /*��λ֮���ٸ�һ��56*/
    SWJ_ClockCycle(56);
    SWJ_SendResetReq(0xE79E);//��λ�ȴ�
    //SWJ_SendResetReq(0xB76D);//���ϰ汾Э��ָ��
    SWJ_ClockCycle(56);
    SWDIO_CLR();
    SWJ_ClockCycle(16);
    /*��ȡIDCODE�Ĵ���ֵ����ȡ�������쳣���߶���ֵ�����򷵻ع���*/
    if(SWJ_ReadDP(DP_IDCODE, &val))  return  RESULT_ERROR;
    return RESULT_OK;
}

u8 SWJ_Init(void)
{
    u32 tmp = 0;
    u32 val = 0;
    SWJ_DataInit();//������ʼ��
    /*JTAG ת�� SWD*/
    SWJ_Reset();
    /*�����־*/
    if(SWJ_WriteDP(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR))    return  RESULT_ERROR;
    /* Ensure CTRL/STAT register selected in DPBANKSEL */
    if(SWJ_WriteDP(DP_SELECT, 0))    return  RESULT_ERROR;
    
    /* Power ups */
    if(SWJ_WriteDP(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ))  return  RESULT_ERROR;
    /*ȷ��system��debug��power-up*/
    do    SWJ_ReadDP(DP_CTRL_STAT, &tmp);
    while((tmp & (CDBGPWRUPACK | CSYSPWRUPACK )) != (CDBGPWRUPACK | CSYSPWRUPACK));
    /*��λ�ں�*/
    if(SWJ_CoreReset())   return  RESULT_ERROR;
    /*��AP_IDR�Ĵ���������ȷֵ�Ƚ�*/
    //if(SWJ_ReadAP(AP_IDR, &val))  return  RESULT_ERROR;

    /*��һ���ֲ�����CM3�����ֲᡷ������ؽ���*/
    return RESULT_OK;
}

/*******************************************************************************/
u8 SWJ_ReadDP(u32 adr, u32 *val)
{
    u8 tmp_in = 0;
    u8 ack = 0;
    u8 err = 0;
    tmp_in = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(adr);
    ack = SWJ_Transfer(tmp_in, val);
    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);
    return err;
}
u8 SWJ_WriteDP(u32 adr, u32 val)
{
    u8 req = 0;
    u8 ack = 0;
    u8 err = 0;

    req = SWD_REG_DP | SWD_REG_W | SWD_REG_ADR(adr);
    ack = SWJ_Transfer(req, &val);

    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);
    return err;
}
/*******************************************************************************/
u8 SWJ_ReadAP(u32 adr, u32 *val)
{
    u8  req = 0 ;
    u8  ack = 0 ;
    u8  err = 0 ;

    u32 apsel = 0;//Ĭ��0��ò��ʵ�ʲ���Ӱ��
    u32 bank_sel = adr & APBANKSEL;//��һ������Ҫ���ã�0xfc�ͼ�F
    if(SWJ_WriteDP(DP_SELECT, apsel | bank_sel))     return  RESULT_ERROR;

    req = SWD_REG_AP | SWD_REG_R | SWD_REG_ADR(adr);
    /* first dummy read */
    ack = SWJ_Transfer(req, val);
    ack = SWJ_Transfer(req, val);//����Ҫ������ʵ�ʲ���ֻ��һ�ε�ֵ��Ϊ��һ�ε�

    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);
    return err;
}
u8 SWJ_WriteAP(u32 adr, u32 val)
{
    u8  ack = 0;
    u8  err = 0;
    u8  req = 0;
//    u32 apsel = 0;
//    u32 bank_sel = adr & APBANKSEL;

//    /* write DP select */
//    if(SWJ_WriteDP(DP_SELECT, apsel | bank_sel))    return  RESULT_ERROR;

    /* write AP data */
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(adr);
    ack = SWJ_Transfer(req, &val);
 
//    /* read DP buff */
//    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
//    ack = SWJ_Transfer(req, &val);
    
    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);

    return err;
}
/*******************************************************************************/
u8 SWJ_ReadBit(void)
{
    u8 bit = 0;
    bit = SWDIO_IN();
    SWJ_ClockCycle(1);
    return bit;
}
void SWJ_WriteBit(u32 bit)
{  
    SWDIO_OUT(bit);
    SWJ_ClockCycle(1);
}
/*******************************************************************************/
void SWJ_SendResetReq(u16 data)
{
    u8 i;

    for(i = 0; i < 16; i++)
    {
        SWJ_WriteBit(((data & 0x1) == 1) ? (1) : (0));
        data >>= 1;
    }
}
/*******************************************************************************/
void SWJ_ClockCycle(u32 num)
{
  u32 i = 0;
  for(i = 0;i < num;i++)
  {
    SWCLK_CLR();
    SWCLK_SET();
  }
}
/*******************************************************************************/
u32 SWJ_Transfer(u8 request, u32 *data)
{
    u8 i = 0;
    u32 ack = 0;
    u32 bit = 0;
    u32 n = 0;
    for(i = 0; i < MAX_SWD_RETRY; i++)//���Դ���
    {
        SWJ_HostReq(request);

        /* Acknowledge response */
        for(n = 0; n < 3; n++)
        {
            bit = SWJ_ReadBit();
            ack  |= bit << n;
        }

        switch(ack)
        {
            case DAP_TRANSFER_OK://��Ӧ��ȷ
                if(SWJ_TransferOK(request,data) != DAP_TRANSFER_ERROR)
                return (ack);

            case DAP_TRANSFER_WAIT:
            case DAP_TRANSFER_FAULT:
                SWJ_TransferFault(request);
                return (ack);

            default:
                break;
        }

        /* Protocol error */
        for(n = 1 + 32 + 1; n; n--)
        {
            SWJ_ClockCycle(1);      /* Back off data phase */
        }

        SWDIO_SET_OUTPUT();
        SWDIO_SET();
        for(n = 0; n < 8; n++)
        {
            SWJ_WriteBit(0);
        }       
        if(ack != DAP_TRANSFER_WAIT | DAP_TRANSFER_FAULT)
          return ack; 
    }
    return ack;//Ϊ������warning
}

void SWJ_HostReq(u8 request)
{
    u32 bit = 0;
    u32 parity = 0 ;
    
    SWDIO_SET_OUTPUT();
    SWDIO_CLR();
    SWJ_ClockCycle(1);
    /* Packet Request */
    parity = 0;
    SWJ_WriteBit(1);         /* Start Bit */
    bit = request & 0x1;
    SWJ_WriteBit(bit);       /* APnDP Bit */
    parity += bit;
    bit = (request >> 1) & 0x1;
    SWJ_WriteBit(bit);       /* RnW Bit */
    parity += bit;
    bit = (request >> 2) & 0x1;
    SWJ_WriteBit(bit);       /* A2 Bit */
    parity += bit;
    bit = (request >> 3) & 0x1;
    SWJ_WriteBit(bit);       /* A3 Bit */
    parity += bit;
    parity %= 2;
    SWJ_WriteBit(parity);    /* Parity Bit */
    SWJ_WriteBit(0);         /* Stop Bit */
    SWJ_WriteBit(1);         /* Park Bit */
    
    /* Turnaround */
    SWJ_ClockCycle(1);
    SWDIO_SET_INPUT();
}

u8 SWJ_TransferOK(u8 request, u32 *data)
{
  u32 bit = 0;
  u32 val = 0;
  u32 n = 0;
  u32 parity = 0;
  u32 ack = 0;
  if(request & DAP_TRANSFER_RnW)  /* read data */
  {
    val = 0;
    parity = 0;

    for(n = 32; n; n--)
    {
      bit = SWJ_ReadBit();  /* Read RDATA[0:31] */
      parity += bit;
      val >>= 1;
      val  |= bit << 31;
    }

    bit = SWJ_ReadBit();    /* Read Parity */

    if((parity ^ bit) & 1)
    {
      ack = DAP_TRANSFER_ERROR;
      return ack;
    }

      if(data) *data = val;

      /* Turnaround */
      SWJ_ClockCycle(1);


      SWDIO_SET_OUTPUT();//SWDIO_SET_OUTPUT
  }
  else    /* write data */
  {
    /* Turnaround */
    SWJ_ClockCycle(1);
    SWDIO_SET_OUTPUT();//SWDIO_SET_OUTPUT
    /* Write data */
    val = *data;
    parity = 0;

    for(n = 0; n < 32; n++)
    {
      bit = (val >> n) & 0x1;
      SWJ_WriteBit(bit); /* Write WDATA[0:31] */
      parity += bit;
    }
    parity %= 2;
    SWJ_WriteBit(parity);/* Write Parity Bit */
  }

  /* Idle cycles */
  SWDIO_SET_OUTPUT();
  SWDIO_SET();
  for(n = 0; n < 8; n++)
  {
    SWJ_WriteBit(0);
  }  
  return DAP_TRANSFER_OK;
}

u8 SWJ_TransferFault(u8 request)
{
  u32 n;
  /* WAIT or FAULT response */
  if(0 && ((request & DAP_TRANSFER_RnW) != 0))
  {
    SWJ_ClockCycle(32);  /* Dummy Read RDATA[0:31] + Parity */
  }

  /* Turnaround */

  SWJ_ClockCycle(1);
  SWDIO_SET_OUTPUT();

  if(0 && ((request & DAP_TRANSFER_RnW) == 0))
  {
    SWJ_ClockCycle(32);  /* Dummy Write WDATA[0:31] + Parity */
  }

  SWDIO_SET();
  for(n = 0; n < 8; n++)
  {
    SWJ_WriteBit(0);
  }  
  return RESULT_OK;
}
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/





/*******************************************************************************/
/************************************target���*********************************/
/*******************************************************************************/
u8  SWJ_ReadData(u32 adr, u32 *val)
{
  // put addr in TAR register
  if(SWJ_WriteAP(AP_TAR,adr))   return  RESULT_ERROR;
  // read data
  if(SWJ_ReadAP(AP_DRW,val))   return  RESULT_ERROR;
  // dummy read
//  u32 req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
//  if(SWJ_Transfer(req,val) != DAP_TRANSFER_OK)   return  RESULT_ERROR;
  return RESULT_OK;
}
u8 SWJ_WriteData(u32 adr, u32 val)
{
  // put addr in TAR register
  if(SWJ_WriteAP(AP_TAR,adr))   return  RESULT_ERROR;
  // write data
  if(SWJ_WriteAP(AP_DRW,val))   return  RESULT_ERROR;
//  // dummy read
//  u32 req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
//  if(SWJ_Transfer(req,&val) != DAP_TRANSFER_OK)   return  RESULT_ERROR;
  return RESULT_OK;
}
/********************************************************************************/
u8 SWJ_ReadByte(u32 addr, u8 *val)
{
  u32 tmp;
  if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE8)) return  RESULT_ERROR;
  if (SWJ_ReadData(addr, &tmp)) return  RESULT_ERROR;
  
  *val = (u8)(tmp >> ((addr & 0x03) << 3));
  return RESULT_OK;
}
u8 SWJ_WriteByte(u32 addr, u8 val)
{
  u32 tmp;

  if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE8)) return  RESULT_ERROR;
  
  tmp = val << ((addr & 0x03) << 3);
  if (SWJ_WriteData(addr, tmp)) return  RESULT_ERROR;
  
  return RESULT_OK;
}
/*******************************************************************************/
u8  SWJ_ReadHalfWord(u32 adr, u32 *val)
{
  /*���ð���С*/
  if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE16)) return  RESULT_ERROR;
  
  
  if (SWJ_ReadData(adr, val)) return  RESULT_ERROR;
  
  return RESULT_OK;
}
u8  SWJ_WriteHalfWord(u32 adr, u32 val)
{
  /*���ð���С*/

  if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE16)) return  RESULT_ERROR;
  
  
  if (SWJ_WriteData(adr, val)) return  RESULT_ERROR;
  
  return RESULT_OK;
}
/******************************************************************************/
/*******************************************************************************/
u8  SWJ_ReadWord(u32 adr, u32 *val)
{
  /*���ð���С*/
  if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32)) return  RESULT_ERROR;
  
  
  if (SWJ_ReadData(adr, val)) return  RESULT_ERROR;
  
  return RESULT_OK;
}
u8  SWJ_WriteWord(u32 adr, u32 val)
{
  /*���ð���С*/

  if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32)) return  RESULT_ERROR;
  
  
  if (SWJ_WriteData(adr, val)) return  RESULT_ERROR;
  
  return RESULT_OK;
}
/******************************************************************************/
u8 SWJ_ReadCoreRegister(u32 registerID, u32 *val)
{
    u32 i;
    u32 timeout = 100;
    /*���ô����Ĵ���*/
    if(SWJ_WriteWord(DBG_CRSR , registerID))   return  RESULT_ERROR;
    
    // wait for S_REGRDY
    for (i = 0; i < timeout; i++) 
    {
        if (SWJ_ReadWord(DBG_HCSR, val))   return  RESULT_ERROR;
        if (*val & S_REGRDY)            break;
    }
    if(i == timeout)    return  RESULT_ERROR;
    if(SWJ_ReadWord(DBG_CRDR , val))   return  RESULT_ERROR;
    return RESULT_OK;
}
u8 SWJ_WriteCoreRegister(u32 registerID, u32 val)
{
    u32 i;
    u32 timeout = 100;
    /*�����д������*/
    if(SWJ_WriteWord(DBG_CRDR , val))   return  RESULT_ERROR;
    /*���üĴ�����д��*/
    if(SWJ_WriteWord(DBG_CRSR , registerID | REGWnR))   return  RESULT_ERROR;
    
     // wait for S_REGRDY
    for (i = 0; i < timeout; i++) 
    {
        if (SWJ_ReadWord(DBG_HCSR, &val))   return  RESULT_ERROR;
        if (val & S_REGRDY)            break;
    }
    if(i == timeout)    return  RESULT_ERROR;
    
    return RESULT_OK; 
}

/*******************************************************************************/
u8 SWJ_ReadMemoryArray(u32 address, u8 *data, u32 size)
{
  u32 n;
  // Read bytes until word aligned
  while ((size > 0) && (address & 0x3)) 
  {
    if (SWJ_ReadByte(address, data)) return  RESULT_ERROR;
    address++;
    data++;
    size--;
  }

  // Read word aligned blocks
  while (size > 3) 
  {
    // Limit to auto increment page size
    n = TARGET_AUTO_INCREMENT_PAGE_SIZE - (address & (TARGET_AUTO_INCREMENT_PAGE_SIZE - 1));
    if (size < n)
    {
      n = size & 0xFFFFFFFC; // Only count complete words remaining
    }

    if (SWJ_ReadBlock(address, data, n)) return  RESULT_ERROR;
    address += n;
    data += n;
    size -= n;
  }

  // Read remaining bytes
  while (size > 0) {
    if (SWJ_ReadByte(address, data)) return  RESULT_ERROR;
    address++;
    data++;
    size--;
  }

  return RESULT_OK;
}
u8 SWJ_WriteMemoryArray(u32 address, u8 *data, u32 size)
{
  u32 n;
  // Write bytes until word aligned
  while ((size > 0) && (address & 0x3)) 
  {
      if (SWJ_WriteByte(address, *data)) {
          return  RESULT_ERROR;
      }
      address++;
      data++;
      size--;
    }

    // Write word aligned blocks
    while (size > 3) {
        // Limit to auto increment page size
        n = TARGET_AUTO_INCREMENT_PAGE_SIZE - (address & (TARGET_AUTO_INCREMENT_PAGE_SIZE - 1));
        if (size < n) 
            n = size & 0xFFFFFFFC; // Only count complete words remaining       

        if (!SWJ_WriteBlock(address, data, n)) {
            return RESULT_OK;
        }

        address += n;
        data += n;
        size -= n;
    }

    // Write remaining bytes
    while (size > 0) {
        if (SWJ_WriteByte(address, *data)) {
            return  RESULT_ERROR;
        }
        address++;
        data++;
        size--;
    }

    return RESULT_OK;
}
/*******************************************************************************/
u8 SWJ_ReadBlock(u32 address, u8 *data, u32 size)
{
    u8  req = 0 ;
    u32 size_in_words;
    u32 i;

    if (size == 0) return  RESULT_ERROR;
   
    size_in_words = size/4;

    if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32)) return  RESULT_ERROR;
    

    // TAR write
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(AP_TAR);
    if(SWJ_Transfer(req,&address) != DAP_TRANSFER_OK)   return  RESULT_ERROR;

    // read data
    req = SWD_REG_AP | SWD_REG_R | SWD_REG_ADR(AP_DRW);
    // dummy read
    if (SWJ_Transfer(req, (u32 *)data) != DAP_TRANSFER_OK) return  RESULT_ERROR;
    
    for (i = 0; i< size_in_words; i++) {
        if (SWJ_Transfer(req, (u32 *)data) != DAP_TRANSFER_OK) {
            return  RESULT_ERROR;
        }
        data += 4;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    if(SWJ_Transfer(req,0) != DAP_TRANSFER_OK)   return  RESULT_ERROR;

    return RESULT_OK;
}
u8 SWJ_WriteBlock(u32 address, u8 *data, u32 size)
{
    u8  req = 0;
    u32 size_in_words;
    u32 i;

    if (size==0)    return  RESULT_ERROR;

    size_in_words = size/4;

    if (SWJ_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32)) return  RESULT_ERROR;
    
    // TAR write
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(AP_TAR);
    if(SWJ_Transfer(req,&address) != DAP_TRANSFER_OK)   return  RESULT_ERROR;

    // DRW write
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(AP_DRW);
    for (i = 0; i < size_in_words; i++) {
        if (SWJ_Transfer(req, (u32 *)data) != DAP_TRANSFER_OK) {
            return  RESULT_ERROR;
        }
        data+=4;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    if(SWJ_Transfer(req,0) != DAP_TRANSFER_OK)   return  RESULT_ERROR;
  
    return RESULT_OK;
}
/*******************************************************************************/
u8 SWJ_WaitUntilHalted(void)
{
  u32 val;
  u32 i,timeout = 100;
  for(i = 0;i < timeout;i++)
  {
    if(SWJ_ReadData(DBG_HCSR, &val))  return  RESULT_ERROR;
    if(val & C_HALT) return RESULT_OK;//Ϊ1ʱ��ֹͣ
  }
  return  RESULT_ERROR;
}
/*******************************************************************************/
u8 SWJ_CoreReset()
{
  u32 i;
  u32 val;
  for(i = 0;i < 5;i++)
  {
    if(SWJ_ReadWord(DBG_HCSR,&val)) return  RESULT_ERROR;
    if(!(val & S_RESET_ST)) break;//������Ϊ0���ʾ�Ѿ���λ
  }
  return RESULT_OK;
}
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/