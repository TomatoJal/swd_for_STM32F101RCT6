#include "swd_dap.h"
#include "swd_app.h"
#include "misc.h"
static uint8 SWD_ReadDP(uint32 adr, uint32 *val);
static uint8 SWD_WriteDP(uint32 adr, uint32 val);
static uint8 SWD_ReadAP(uint32 adr, uint32 *val);
static uint8 SWD_WriteAP(uint32 adr, uint32 val);
static uint8 SWD_ReadData(uint32 adr, uint32 *val);
static uint8 SWD_WriteData(uint32 adr, uint32 val);
/*****************************************************************************************
函数名称: SWD_ReadDP
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
static uint8 SWD_ReadDP(uint32 adr, uint32 *val)
{
    uint8 tmp_in = 0;
    uint8 ack = 0;
    uint8 err = 0;
    tmp_in = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(adr);
    ack = SWD_Transfer(tmp_in, val);
    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);
    return err;
}
/*****************************************************************************************
函数名称: SWD_WriteDP
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
static uint8 SWD_WriteDP(uint32 adr, uint32 val)
{
    uint8 req = 0;
    uint8 ack = 0;
    uint8 err = 0;
    req = SWD_REG_DP | SWD_REG_W | SWD_REG_ADR(adr);
    ack = SWD_Transfer(req, &val);
    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);
    return err;
}
/*****************************************************************************************
函数名称: SWD_ReadAP
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
static uint8 SWD_ReadAP(uint32 adr, uint32 *val)
{
    uint8  req = 0 ;
    uint8  ack = 0 ;
    uint8  err = 0 ;

    uint32 apsel = 0;//默认0，貌似实际并不影响
    uint32 bank_sel = adr & APBANKSEL;//这一半字需要设置，0xfc就记F
    Check(SWD_WriteDP(DP_SELECT, apsel | bank_sel)) ;
    req = SWD_REG_AP | SWD_REG_R | SWD_REG_ADR(adr);
    /* first dummy read */
    ack = SWD_Transfer(req, val);
    ack = SWD_Transfer(req, val);//这里要读两次实际测试只读一次的值仍为上一次的
    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);
    return err;
}
/*****************************************************************************************
函数名称: SWD_WriteAP
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_WriteAP(uint32 adr, uint32 val)
{
    uint8  ack = 0;
    uint8  err = 0;
    uint8  req = 0;
    /* write AP data */
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(adr);
    ack = SWD_Transfer(req, &val);
    (ack == DAP_TRANSFER_OK) ? (err = 0) : (err = 1);

    return err;
}
/*****************************************************************************************
函数名称: SWD_ReadData
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
static uint8 SWD_ReadData(uint32 adr, uint32 *val)
{
    Check(SWD_WriteAP(AP_TAR,adr));
    Check(SWD_ReadAP(AP_DRW,val));
    return RESULT_OK;
}
/*****************************************************************************************
函数名称: SWD_WriteData
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
static uint8 SWD_WriteData(uint32 adr, uint32 val)
{
    Check(SWD_WriteAP(AP_TAR,adr));
    Check(SWD_WriteAP(AP_DRW,val));
    return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WByte
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_ReadByte(uint32 addr, uint8 *val)
{
    uint32 tmp;
    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE8));
    Check(SWD_ReadData(addr, &tmp));
    *val = (uint8)(tmp >> ((addr & 0x03) << 3));
    return RESULT_OK;
}
uint8 SWD_WriteByte(uint32 addr, uint8 val)
{
    uint32 tmp;
    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE8));
    tmp = val << ((addr & 0x03) << 3);
    Check(SWD_WriteData(addr, tmp));
    return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WHalfByte
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8  SWD_ReadHalfWord(uint32 adr, uint32 *val)
{
    /*配置包大小*/
    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE16));
    Check(SWD_ReadData(adr, val));
    return RESULT_OK;
}

uint8  SWD_WriteHalfWord(uint32 adr, uint32 val)
{
    /*配置包大小*/
    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE16));
    Check(SWD_WriteData(adr, val));
    return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WWord
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8  SWD_ReadWord(uint32 adr, uint32 *val)
{
    /*配置包大小*/
    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32));
    Check(SWD_ReadData(adr, val));
    return RESULT_OK;
}
uint8  SWD_WriteWord(uint32 adr, uint32 val)
{
    /*配置包大小*/
    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32));
    Check(SWD_WriteData(adr, val));
    return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WCoreRegister
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_ReadCoreRegister(uint32 registerID, uint32 *val)
{
    uint32 i;
    uint32 timeout = 100;
    /*配置待读寄存器*/
    Check(SWD_WriteWord(DBG_CRSR , registerID));
    
    for (i = 0; i < timeout; i++) 
    {
        Check(SWD_ReadWord(DBG_HCSR, val));
        if (*val & S_REGRDY)            break;
    }
    if(i == timeout)    return  RESULT_ERROR;
    Check(SWD_ReadWord(DBG_CRDR , val));
    return RESULT_OK;
}
uint8 SWD_WriteCoreRegister(uint32 registerID, uint32 val)
{
    uint32 i;
    uint32 timeout = 100;
    /*放入待写入数据*/
    Check(SWD_WriteWord(DBG_CRDR , val));
    /*配置寄存器待写入*/
    Check(SWD_WriteWord(DBG_CRSR , registerID | REGWnR));
    
    for (i = 0; i < timeout; i++) 
    {
        Check(SWD_ReadWord(DBG_HCSR, &val));
        if (val & S_REGRDY)            break;
    }
    if(i == timeout)    return  RESULT_ERROR;
    
    return RESULT_OK; 
}

/*****************************************************************************************
函数名称: SWD_R/WMemoryArray
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_ReadMemoryArray(uint32 address, uint8 *data, uint32 size)
{
  uint32 n;
  // Read bytes until word aligned
  while ((size > 0) && (address & 0x3)) 
  {
    Check(SWD_ReadByte(address, data));
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

    Check(SWD_ReadBlock(address, data, n));
    address += n;
    data += n;
    size -= n;
  }

  // Read remaining bytes
  while (size > 0) {
    Check(SWD_ReadByte(address, data));
    address++;
    data++;
    size--;
  }

  return RESULT_OK;
}
uint8 SWD_WriteMemoryArray(uint32 address, uint8 *data, uint32 size)
{
  uint32 n;
  // Write bytes until word aligned
  while ((size > 0) && (address & 0x3)) 
  {
      Check(SWD_WriteByte(address, *data));
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

        Check(SWD_WriteBlock(address, data, n));
        address += n;
        data += n;
        size -= n;
    }

    // Write remaining bytes
    while (size > 0) {
        Check(SWD_WriteByte(address, *data));
        address++;
        data++;
        size--;
    }

    return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WBlock
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_ReadBlock(uint32 address, uint8 *data, uint32 size)
{
    uint8  req = 0 ;
    uint32 size_in_words;
    uint32 i;

    if (size == 0) return  RESULT_ERROR;
   
    size_in_words = size/4;

    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32));
    
    // TAR write
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(AP_TAR);
    Check(SWD_Transfer(req,&address) != DAP_TRANSFER_OK);

    // read data
    req = SWD_REG_AP | SWD_REG_R | SWD_REG_ADR(AP_DRW);
    // dummy read
    Check(SWD_Transfer(req, (uint32 *)data) != DAP_TRANSFER_OK);
    
    for (i = 0; i< size_in_words; i++) {
        Check(SWD_Transfer(req, (uint32 *)data) != DAP_TRANSFER_OK);
        data += 4;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    Check(SWD_Transfer(req,0) != DAP_TRANSFER_OK);

    return RESULT_OK;
}
uint8 SWD_WriteBlock(uint32 address, uint8 *data, uint32 size)
{
    uint8  req = 0;
    uint32 size_in_words;
    uint32 i;

    if (size==0)    return  RESULT_ERROR;

    size_in_words = size/4;

    Check(SWD_WriteAP(AP_CSW, CSW_VALUE | CSW_SIZE32));
    
    // TAR write
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(AP_TAR);
    Check(SWD_Transfer(req,&address) != DAP_TRANSFER_OK);

    // DRW write
    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(AP_DRW);
    for (i = 0; i < size_in_words; i++) {
        Check(SWD_Transfer(req, (uint32 *)data) != DAP_TRANSFER_OK);
        data+=4;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
    Check(SWD_Transfer(req,0) != DAP_TRANSFER_OK);
  
    return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WBlock
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
static uint8 SWD_WaitUntilHalted(void)
{
  uint32 val;
  uint32 i,timeout = 100;
  for(i = 0;i < timeout;i++)
  {
    Check(SWD_ReadData(DBG_HCSR, &val));
    if(val & C_HALT) return RESULT_OK;//为1时才停止
  }
  return  RESULT_ERROR;
}

/*****************************************************************************************
函数名称: SWD_R/WBlock
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_CoreReset()
{
  uint32 i;
  uint8 val;
  for(i = 0;i < 5;i++)
  {
    Check(SWD_ReadByte(DBG_HCSR+0x03,&val));
    if(!((uint32)(val<<24) & S_RESET_ST)) break;//读两次为0后表示已经复位
  }
  
//  RELEASE_MCU_RESET();
//  MCUDelayMs(50);
//  HOLD_MCU_RESET();
  return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WBlock
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_CoreStop(void)
{
  if(SWD_WriteWord(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT))  return  RESULT_ERROR;
  if(SWD_WaitUntilHalted())   return RESULT_ERROR;
  return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_R/WBlock
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_CoreStart(void)
{
  if(SWD_WriteWord(DBG_HCSR, DBGKEY | C_DEBUGEN))  return  RESULT_ERROR;
  return RESULT_OK;
}

/*****************************************************************************************
函数名称: SWD_DataInit
入口参数: 无
出口参数: 无
返回参数：无
函数功能：无
*****************************************************************************************/
uint8 SWD_Init(void)
{
    uint32 tmp = 0,val = 0;
    /*JTAG 转到 SWD*/
    SWD_ResetCycle();
    
    /*读取IDCODE寄存器值若读取过程有异常或者读出值不对则返回故障*/
    Check(SWD_ReadDP(DP_IDCODE, &val));
    /*清除标志*/
    Check(SWD_WriteDP(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR));
    /* Ensure CTRL/STAT register selected in DPBANKSEL */
    Check(SWD_WriteDP(DP_SELECT, 0));
        
    /* Power ups */
    Check(SWD_WriteDP(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ));
    /*确保system与debug都power-up*/
    do{    
        SWD_ReadDP(DP_CTRL_STAT, &tmp);
        tmp++;
        if(tmp == 20)
        {
            break;
        }
    }
    while((tmp & (CDBGPWRUPACK | CSYSPWRUPACK )) != (CDBGPWRUPACK | CSYSPWRUPACK));
    /*复位内核*/
    Check(SWD_CoreReset());
    /*读AP_IDR寄存器并与正确值比较*/
    //0x0477 0021
    //0000 Revision,第一次执行 AP 设计时该位为零。在对设计每作一次较大的修改时，对该位进行更新
    //0100 JEP-106 continuation code 对于一个采用 ARM 设计的 AP， [27:24]值为 0b0100， 0x4
    //0111 011 JEP-106 identify code 对于一个采用 ARM 设计的 AP， [23:17]值为 0b0111011， 0x3B
    //1 Class 0b1，这个 AP 是存储器访问端口
    //0000 0000 保留， SBZ
    //0010 AP Variant 0x1： Cortex-M0 变量
    //0001 AP Type 0x1： AMBA AHB 总线
//    Check(SWD_ReadAP(AP_IDR, &val));
    /*这一部分操作《CM3技术手册》中有相关讲解*/
    return RESULT_OK;
}



