#include "swd_dap.h"
#include "swd_app.h"
#include "ict_communication.h"
#include "m25pxx.h"
#include "misc.h"

static void SWD_ClockCycle(uint32 num);
static uint8  SWD_ReadBit(void);
static uint8  SWD_WriteBit(uint32 bit);
static void  SWD_HostReq(uint8 request);
static uint8  SWD_TransferOK(uint8 request, uint32 *data);
static uint8  SWD_TransferFault(uint8 request);
static void  SWD_SendResetReq(uint16 data);
/*****************************************************************************************
��������: SWD_ClockCycle
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���������
*****************************************************************************************/
static void SWD_ClockCycle(uint32 num)
{
    uint32 i = 0;
    for(i = 0;i < num;i++)
    {
        SWCLK_CLR();
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");        
        SWCLK_SET();
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
//        asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");  
    }
}
/*****************************************************************************************
��������: SWD_ReadBit
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 SWD_ReadBit(void)
{
    uint8 bit = 0;
    bit = SWDIO_IN();
    SWD_ClockCycle(1);
    return bit;
}
/*****************************************************************************************
��������: SWD_WriteBit
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 SWD_WriteBit(uint32 bit)
{  
    SWDIO_OUT(bit);
    SWD_ClockCycle(1);
    return 0;
}
/*****************************************************************************************
��������: SWD_HostReq
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ�����������֡
*****************************************************************************************/
static void SWD_HostReq(uint8 request)
{
    uint32 bit = 0;
    uint32 parity = 0 ;
    
    SWDIO_SET_OUTPUT();
    SWDIO_CLR();
    SWD_ClockCycle(1);
    /* Packet Request */
    parity = 0;
    SWD_WriteBit(1);         /* Start Bit */
    bit = request & 0x1;
    SWD_WriteBit(bit);       /* APnDP Bit */
    parity += bit;
    bit = (request >> 1) & 0x1;
    SWD_WriteBit(bit);       /* RnW Bit */
    parity += bit;
    bit = (request >> 2) & 0x1;
    SWD_WriteBit(bit);       /* A2 Bit */
    parity += bit;
    bit = (request >> 3) & 0x1;
    SWD_WriteBit(bit);       /* A3 Bit */
    parity += bit;
    parity %= 2;
    SWD_WriteBit(parity);    /* Parity Bit */
    SWD_WriteBit(0);         /* Stop Bit */
    SWD_WriteBit(1);         /* Park Bit */
    
    /* Turnaround */
    SWD_ClockCycle(1);
    SWDIO_SET_INPUT();
}
/*****************************************************************************************
��������: SWD_TransferOK
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 SWD_TransferOK(uint8 request, uint32 *data)
{
  uint32 bit = 0;
  uint32 val = 0;
  uint32 n = 0;
  uint32 parity = 0;
  uint32 ack = 0;
  if(request & DAP_TRANSFER_RnW)  /* read data */
  {
    val = 0;
    parity = 0;

    for(n = 32; n; n--)
    {
      bit = SWD_ReadBit();  /* Read RDATA[0:31] */
      parity += bit;
      val >>= 1;
      val  |= bit << 31;
    }

    bit = SWD_ReadBit();    /* Read Parity */

    if((parity ^ bit) & 1)
    {
      ack = DAP_TRANSFER_ERROR;
      return ack;
    }

      if(data) *data = val;

      /* Turnaround */
      SWD_ClockCycle(1);


      SWDIO_SET_OUTPUT();//SWDIO_SET_OUTPUT
  }
  else    /* write data */
  {
    /* Turnaround */
    SWD_ClockCycle(1);
    SWDIO_SET_OUTPUT();//SWDIO_SET_OUTPUT
    /* Write data */
    val = *data;
    parity = 0;

    for(n = 0; n < 32; n++)
    {
      bit = (val >> n) & 0x1;
      SWD_WriteBit(bit); /* Write WDATA[0:31] */
      parity += bit;
    }
    parity %= 2;
    SWD_WriteBit(parity);/* Write Parity Bit */
  }

  /* Idle cycles */
  SWDIO_SET_OUTPUT();
  SWDIO_SET();
  for(n = 0; n < 8; n++)
  {
    SWD_WriteBit(0);
  }  
  return DAP_TRANSFER_OK;
}
/*****************************************************************************************
��������: SWD_TransferFault
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static uint8 SWD_TransferFault(uint8 request)
{
  uint32 n;
  /* WAIT or FAULT response */
  if(0 && ((request & DAP_TRANSFER_RnW) != 0))
  {
    SWD_ClockCycle(32);  /* Dummy Read RDATA[0:31] + Parity */
  }

  /* Turnaround */

  SWD_ClockCycle(1);
  SWDIO_SET_OUTPUT();

  if(0 && ((request & DAP_TRANSFER_RnW) == 0))
  {
    SWD_ClockCycle(32);  /* Dummy Write WDATA[0:31] + Parity */
  }

  SWDIO_SET();
  for(n = 0; n < 8; n++)
  {
    SWD_WriteBit(0);
  }  
  return RESULT_OK;
}

/*****************************************************************************************
��������: SWD_Transfer
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
uint32 SWD_Transfer(uint8 request, uint32 *data)
{
    uint8 i = 0;
    uint32 ack = 0;
    uint32 bit = 0;
    uint32 n = 0;
    for(i = 0; i < MAX_SWD_RETRY; i++)//���Դ���
    {
        SWD_HostReq(request);

        /* Acknowledge response */
        for(n = 0; n < 3; n++)
        {
            bit = SWD_ReadBit();
            ack  |= bit << n;
        }

        switch(ack)
        {
            case DAP_TRANSFER_OK://��Ӧ��ȷ
                if(SWD_TransferOK(request,data) != DAP_TRANSFER_ERROR)
                return (ack);

            case DAP_TRANSFER_WAIT:
            case DAP_TRANSFER_FAULT:
                SWD_TransferFault(request);
                return (ack);

            default:
                break;
        }

        /* Protocol error */
        for(n = 1 + 32 + 1; n; n--)
        {
            SWD_ClockCycle(1);      /* Back off data phase */
        }

        SWDIO_SET_OUTPUT();
        SWDIO_SET();
        for(n = 0; n < 8; n++)
        {
            SWD_WriteBit(0);
        }       
        if(ack != DAP_TRANSFER_WAIT | DAP_TRANSFER_FAULT)
          return ack; 
    }
    return ack;
}
/*****************************************************************************************
��������: SWD_DataInit
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
static void SWD_SendResetReq(uint16 data)
{
    uint8 i;

    for(i = 0; i < 16; i++)
    {
        SWD_WriteBit(((data & 0x1) == 1) ? (1) : (0));
        data >>= 1;
    }
}
/*****************************************************************************************
��������: SWD_ResetCycle()
��ڲ���: ��
���ڲ���: ��
���ز�������
�������ܣ���
*****************************************************************************************/
void SWD_ResetCycle()
{
    SWDIO_SET();
    /*��ʼ53��������һ��0xE79E���и�λ*/ 
    /*��λ֮���ٸ�һ��56*/
    SWD_ClockCycle(56);
    SWD_SendResetReq(0xE79E);//��λ�ȴ�
    //SWD_SendResetReq(0xB76D);//���ϰ汾Э��ָ��
    SWD_ClockCycle(56);
    SWDIO_CLR();
    SWD_ClockCycle(16);
}