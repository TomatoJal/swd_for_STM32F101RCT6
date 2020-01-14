/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "progmcu.h"
#include "misc.h"
#include "dwn_assist.h"
#include "m25pxx.h"
#include "ict_communication.h"

/* Private typedef -----------------------------------------------------------*/

/* For the command details, please refer to the document <STM32 system memory boot mode>. */
typedef enum
{
    GET_VERSION = 0x00,
    GET_VER_STATUS = 0x01,
    GET_CHIP_ID = 0x02,
    READ_MEMORY = 0x11,
    GO_CMD = 0x21,
    WRITE_MEMORY = 0x31,
    ERASE = 0x43,
    WRITE_PROTECT = 0x63,
    WRITE_UNPROTECT = 0x73,
    READOUT_PROTECT = 0x82,
    READOUT_UNPROTECT = 0x92,
} BootloaderCommand;

/* Private define ------------------------------------------------------------*/
#define PROGMCU_ACK             0x79
#define PROGMCU_NACK            0x1F
#define PROG_USART              USART1

#define RECEIVE_ACK             0x00
#define RECEIVE_DATA_LENGTH     0x01
#define RECEIVE_DATA_CONTEXT    0x02

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
tTransceiverFSM ProgMCUTransmit;
tTransceiverFSM ProgMCUReceive;
bool UsartState;

static bool AckFlag;
static bool NackFlag;
static bool InfoFlag;
static vu16 RespTimeoutCnt;

extern vu16 AssistRspFlag;
/* Private function prototypes -----------------------------------------------*/
static bool ProgMCUDataRequest(const u8 *dat);
static bool ProgMCUDataIndication(tTransceiverFSM *receiver, const u8 dat);
static bool ProgMCUReadBootVersion(void);
static u8   ProgMCUCalChksum(const u8 *dat, u8 length);
static void ProgMCUSetTimeout(u16 timeout);
static u16  ProgMCUGetTimeout(void);
static void ClearACKandNACK(void);
void ProgMCUUsartInterrupt(void);
/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
* Function Name  : ProgramMCUInit
* Description    : Init program MCU usart.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ProgMCUInit(void)
{
    NVIC_InitTypeDef  NVIC_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* Enable USART1 clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    USART_DeInit(PROG_USART);

    /* Enable the USART1 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 15;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    /* Configure USART1 Tx (PA.9) as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed= GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure USART1 Rx (PA.10) as input floating */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* USART1 configuration ----------------------------------------------------*/
    /* USART1 configured as follow:
        - BaudRate = 115200 baud
        - Word Length = 8 Bits
        - One Stop Bit
        - EVEN parity
        - Receive and transmit enabled
        - Hardware flow control disabled (RTS and CTS signals)
    */
    USART_InitStructure.USART_BaudRate = 57600;
    USART_InitStructure.USART_WordLength = USART_WordLength_9b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_Even;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    USART_Init(PROG_USART, &USART_InitStructure);
    USART_Cmd(PROG_USART, ENABLE);
    USART_ITConfig(PROG_USART, USART_IT_RXNE, ENABLE);

    UsartState = FALSE;
}

/*******************************************************************************
* Function Name  : ProgMCUStart
* Description    : Start to program the MCU.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
#define RETRY_LIMIT     5
bool ProgMCUStart(void)
{
    u32 RetrySend7F = 0;
    u32 RetryReadVersion = 0;

    ProgMCUInit();

STEP1:

    RetrySend7F++;
    RetryReadVersion++;

    // Set Boot0 Pin
    HOLD_MCU_RESET();
    BOOT_FROM_SYSMEM();

    // wait for 1ms.
    MCUDelayMs(2);

    // release the MCU reset
    RELEASE_MCU_RESET();

    // wait for 20ms
    MCUDelayMs(150);
    FEED_DOG();

    // send the char 0x7F
    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    USART_SendData(PROG_USART, 0x7F);

    FEED_DOG();
    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            if(RetrySend7F > RETRY_LIMIT)
            {
                return FALSE;
            }
            DPRINTF("\r\nMPB has failed to send 0x7F. MPB sends 0x7F %d times.", RetrySend7F);
            goto STEP1;
        }
    }

    // read boot loader version
    if(!ProgMCUReadBootVersion())
    {
        if(RetryReadVersion >= RETRY_LIMIT)
        {
            return FALSE;
        }
        DPRINTF("\r\nMPB has failed to read bootloader version. MPB has tried %d times.", RetryReadVersion);
        goto STEP1;
    }

    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUReadBootVersion
* Description    : Read the boot loader version.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
const u8 ReadBootCmd[] = {2, GET_VERSION, 0xFF ^ GET_VERSION};
static bool ProgMCUReadBootVersion(void)
{
    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(ReadBootCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            return FALSE;
        }
    }

    // wait for information
    ClearACKandNACK();
    ProgMCUReceive.status = RECEIVE_DATA_LENGTH;
    ProgMCUSetTimeout(10);

    while(!InfoFlag)
    {
        if(0 == ProgMCUGetTimeout())
        {
            return FALSE;
        }
    }

    // print the boot loader information
    DPRINTF("\r\nThe boot loader version is V%x.%x .", ProgMCUReceive.buf[0] >> 4, ProgMCUReceive.buf[0] & 0x0F);
    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUWriteMemory
* Description    : Write data to mcu memory.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- write memory sucess.
                         FALSE -- write memory fails.
*******************************************************************************/
const u8 WriteMemoryCmd[] = {2, WRITE_MEMORY, 0xFF ^ WRITE_MEMORY};
/*
bool ProgMCUWriteMemory(u32 addr, void *dat, u8 length)
{
    u8 data[4] = {0x01, 0x02, 0x03, 0x04};
    u8 buf[250];
  
  
    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(WriteMemoryCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWriting memory fail! Command No ACK!");
            return FALSE;
        }
    }

    // write address
    buf[0] = 5;
    buf[1] = 0x20;//(addr >> 24) & 0xFF;
    buf[2] = 0x00;//(addr >> 16) & 0xFF;
    buf[3] = 0x30;//(addr >> 8)  & 0xFF;
    buf[4] = 0x00;(addr >> 0)  & 0xFF;
    buf[5] = 0x10;//buf[1] ^ buf[2] ^ buf[3] ^ buf[4];

    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(buf);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWriting memory fail! Write Addr No ACK!");
            return FALSE;
        }
    }

    // write the real data
    buf[0] = 242;//length + 2;
    buf[1] = 239;//length - 1;
    memset(&buf[2], 0x12, 240);

    buf[242] = ProgMCUCalChksum(&buf[1], 5);

    ClearACKandNACK();
    // 1000ms is too big. Change to another small one.
    ProgMCUSetTimeout(1000);
    //ProgMCUSetTimeout(200);
    ProgMCUDataRequest(buf);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWriting memory fail! Write Data No ACK!");
            return FALSE;
        }
    }
    return TRUE;
}
*/

bool ProgMCUWriteMemory(u32 addr, void *dat, u8 length)
{
    u8 buf[260];

    // cmd
    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(WriteMemoryCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWriting memory fail! Command No ACK!");
            return FALSE;
        }
    }

    // write address
    buf[0] = 5;
    buf[1] = (addr >> 24) & 0xFF;
    buf[2] = (addr >> 16) & 0xFF;
    buf[3] = (addr >> 8)  & 0xFF;
    buf[4] = (addr >> 0)  & 0xFF;
    buf[5] = buf[1] ^ buf[2] ^ buf[3] ^ buf[4];

    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(buf);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWriting memory fail! Write Addr No ACK!");
            return FALSE;
        }
    }

    // write the real data
    buf[0] = length + 2;
    buf[1] = length - 1;
    memcpy(&buf[2], dat, length);

    buf[length+2] = ProgMCUCalChksum(&buf[1], length + 1);

    ClearACKandNACK();
    // 1000ms is too big. Change to another small one.
    ProgMCUSetTimeout(1000);
    //ProgMCUSetTimeout(200);
    ProgMCUDataRequest(buf);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWriting memory fail! Write Data No ACK!");
            return FALSE;
        }
    }

    return TRUE;
}


/*******************************************************************************
* Function Name  : ProgMCUReadMemory
* Description    : Read data from mcu memory.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- read memory sucess.
                         FALSE -- read memory fails.
*******************************************************************************/
const u8 ReadMemoryCmd[] = {2, READ_MEMORY, 0xFF ^ READ_MEMORY};
bool ProgMCUReadMemory(u32 addr, void *dat, u8 length)
{
    u8 buf[260];

    // cmd
    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(ReadMemoryCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nReading memory fail! Command No ACK!");
            return FALSE;
        }
    }

    // write address
    buf[0] = 5;
    buf[1] = (addr >> 24) & 0xFF;
    buf[2] = (addr >> 16) & 0xFF;
    buf[3] = (addr >> 8)  & 0xFF;
    buf[4] = (addr >> 0)  & 0xFF;
    buf[5] = buf[1] ^ buf[2] ^ buf[3] ^ buf[4];

    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(buf);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nReading memory fail! Write Addr No ACK!");
            return FALSE;
        }
    }

    // wait for data

    // wait for checksum
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nReading memory fail! Wait for data No ACK!");
            return FALSE;
        }
    }

    return TRUE;
}

bool SetFlashWriteByWord(void)
{
    u32 buf = 0xFFFFFF03;
    
    return ProgMCUWriteMemory(UUP_VOLTAGE_RANGE_ADDRESS, &buf, 4);
}

/*******************************************************************************
* Function Name  : ProgMCUDwldAssist
* Description    : Write data to mcu memory.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- write memory sucess.
                         FALSE -- write memory fails.
*******************************************************************************/
bool ProgMCUDwldAssist(void)
{
    u8 buf[256];
    u32 FlashAddr = ASSPROG_BASE_ADDRESS;
    u32 RamAddr   = UUP_PROGRAM_START_ADDRESS;
    u32 ImageLength = 0;
    u32 DownloadSize;

    ImageLength = ICT_GetImageLength(ASSIST_IMAGE);

    if(ImageLength >= ASSIST_IMAGE_SIZE)
    {
        DPRINTF("\r\nThe downloading assist image is too large, it is %d bytes!!!", ImageLength);
        return FALSE;
    }

    /*
    if(ImageLength < 3 * 1024)
    {
        DPRINTF("\r\nThe downloading assist image is too small, it is %d bytes!!!", ImageLength);
        return FALSE;
    }
    */

    while(ImageLength > 0)
    {
        if(ImageLength > 240)
        {
            DownloadSize = 240;
        }
        else
        {
            DownloadSize = ImageLength;
        }

        if(!M25_ReadBytes(FlashAddr, buf, DownloadSize))
        {
            return FALSE;
        }

        if(!ProgMCUWriteMemory(RamAddr, buf, DownloadSize))
        {
            return FALSE;
        }

        FlashAddr   += DownloadSize;
        RamAddr     += DownloadSize;
        ImageLength -= DownloadSize;

        FEED_DOG();
    }

    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUWriteMemory
* Description    : Write data to mcu memory.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- write memory sucess.
                         FALSE -- write memory fails.
*******************************************************************************/
const u8 GoInstructionCmd[] = {2, GO_CMD, 0xFF ^ GO_CMD};
bool ProgMCUGoInstruction(u32 addr)
{
    u8 buf[10];

    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(GoInstructionCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nGo instruction fail! Command No ACK!");
            return FALSE;
        }
    }

    // write address, MSB first
    buf[0] = 5;
    buf[1] = (addr >> 24) & 0xFF;
    buf[2] = (addr >> 16) & 0xFF;
    buf[3] = (addr >> 8)  & 0xFF;
    buf[4] = (addr >> 0)  & 0xFF;
    buf[5] = buf[1] ^ buf[2] ^ buf[3] ^ buf[4];

    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(buf);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nGo instruction fail! Write address No ACK!");
            return FALSE;
        }
    }

    return TRUE;
}

/*******************************************************************************
* Function Name  : EraseAllFlash
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- write flash sucess.
                         FALSE -- write flash fails.
*******************************************************************************/
//const u8 EraseAllFlashCmd1[] = {2, ERASE, 0xFF ^ ERASE};
const u8 EraseAllFlashCmd1[] = {2, 0x44, 0xBB};
const u8 EraseAllFlashCmd2[] = {3, 0xFF, 0xFF, 0x00};

bool ProgMCUEraseAllFlash(void)
{
    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(EraseAllFlashCmd1);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nThe whole flash erase fail! Command No ACK!");
            return FALSE;
        }
        FEED_DOG();
    }

    ClearACKandNACK();

    // fine tune this timeout value.
    ProgMCUSetTimeout(20000);
    ProgMCUDataRequest(EraseAllFlashCmd2);

    FEED_DOG();

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nThe whole flash erase fail! Erase No ACK!");
            return FALSE;
        }
        FEED_DOG();
    }

    // print the information
    DPRINTF("\r\nThe whole flash erase done!");

    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUWriteUnprotect
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- write flash sucess.
                         FALSE -- write flash fails.
*******************************************************************************/
const u8 WriteUnprotectCmd[3] = {2, WRITE_UNPROTECT, 0xFF ^ WRITE_UNPROTECT};

bool ProgMCUWriteUnprotect(void)
{
    ClearACKandNACK();
    ProgMCUSetTimeout(100);
    ProgMCUDataRequest(WriteUnprotectCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWrite unprotect failed! Command no ACK !");
            return FALSE;
        }
    }

    ClearACKandNACK();
    ProgMCUSetTimeout(150);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nWrite unprotect failed! Operation no ACK !");
            return FALSE;
        }
    }

    // print the information
    DPRINTF("\r\nWrite unprotect command done!");

    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUReadOutUnprotect
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- write flash sucess.
                         FALSE -- write flash fails.
*******************************************************************************/
const u8 ReadOutUnprotectCmd[] = {2, READOUT_UNPROTECT, 0xFF ^ READOUT_UNPROTECT};

bool ProgMCUReadOutUnprotect(void)
{
    ClearACKandNACK();
    ProgMCUSetTimeout(100);
    ProgMCUDataRequest(ReadOutUnprotectCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nRead out unprotect failed! Command no ACK !");
            return FALSE;
        }
    }

    ClearACKandNACK();
    ProgMCUSetTimeout(5000);
    //FEED_DOG();
    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nRead out unprotect failed! Operation no ACK !");
            return FALSE;
        }
        FEED_DOG();  
    }

    // print the information
    DPRINTF("\r\nRead out unprotect command done!");

    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUReadOutProtect
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : bool, TRUE  -- write flash sucess.
                         FALSE -- write flash fails.
*******************************************************************************/
const u8 ReadOutProtectCmd[] = {2, READOUT_PROTECT, 0xFF ^ READOUT_PROTECT};
bool ProgMCUReadOutProtect(void)
{
    ClearACKandNACK();
    ProgMCUSetTimeout(10);
    ProgMCUDataRequest(ReadOutProtectCmd);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nRead out protect failed! Command no ACK !");
            return FALSE;
        }
    }

    ClearACKandNACK();
    ProgMCUSetTimeout(100);

    // wait for ACK
    while(!AckFlag)
    {
        if((0 == ProgMCUGetTimeout()) || (NackFlag))
        {
            DPRINTF("\r\nRead out protect failed! Operation no ACK !");
            return FALSE;
        }
    }

    // print the information
    DPRINTF("\r\nRead out protect command done!");

    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUCalChksum
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : u8, the data XOR checksum.
*******************************************************************************/
static u8 ProgMCUCalChksum(const u8 *dat, u8 length)
{
    u8 chksum = 0x00;

    while(length--)
    {
        chksum ^= *dat;
        dat++;
    }

    return chksum;
}

/*******************************************************************************
* Function Name  : ProgMCUTimeout
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ProgMCUTimeout(void)
{
    if(RespTimeoutCnt)
    {
        RespTimeoutCnt--;
    }

    if(ProgMCUTransmit.Timeout)
    {
        ProgMCUTransmit.Timeout--;

        if(0 == ProgMCUTransmit.Timeout)
        {
            ProgMCUTransmit.status = FALSE;

            /* This function uses very frequently. */
            //USART_ITConfig(PROG_USART, USART_IT_TXE, DISABLE);
            PROG_USART->CR1 &= ~(1UL << 7);
        }
    }

    if(ProgMCUReceive.Timeout)
    {
        ProgMCUReceive.Timeout--;

        if(0 == ProgMCUReceive.Timeout)
        {
            ProgMCUReceive.status = RECEIVE_ACK;
        }
    }
}

/*******************************************************************************
* Function Name  : SetProgMCUTimeout
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void ProgMCUSetTimeout(u16 timeout)
{
    DisAllInterrupt();
    RespTimeoutCnt = timeout;
    EnAllInterrupt();
}

/*******************************************************************************
* Function Name  : GetProgMCUTimeout
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static u16 ProgMCUGetTimeout(void)
{
    return RespTimeoutCnt;
}

/*******************************************************************************
* Function Name  : ClearACKandNACK
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void ClearACKandNACK(void)
{
    DisAllInterrupt();
    AckFlag = FALSE;
    NackFlag = FALSE;
    InfoFlag = FALSE;
    ProgMCUReceive.status = RECEIVE_ACK;
    EnAllInterrupt();
}

/*******************************************************************************
* Function Name  : ProgMCUDataRequest
* Description    : Read the boot loader version.
* Input          : const u8 *dat
                   dat[0]       ---- data length, exclude itself.
                   dat[1...]    ---- data string.
* Output         : None
* Return         : bool
                   True  ---- Request sucess
                   False ---- Request fails
*******************************************************************************/
static bool ProgMCUDataRequest(const u8 *dat)
{
    if(ProgMCUTransmit.status == TRUE)
    {
        return FALSE;
    }

    ProgMCUTransmit.FrameLen = dat[0];
    ProgMCUTransmit.ptr = 0;
    ProgMCUTransmit.status = TRUE;
    ProgMCUTransmit.Timeout = 20;

    memcpy(ProgMCUTransmit.buf, &dat[1], dat[0]);

    /* This function uses very frequently. */
    //USART_ITConfig(PROG_USART, USART_IT_TXE, ENABLE);
    PROG_USART->CR1 |= (1UL << 7);

    return TRUE;
}

/*******************************************************************************
* Function Name  : ProgMCUDataIndication
* Description    : Read the boot loader version.
* Input          : tTransceiverFSM *receiver    ---- Receiving finite state machine.
                   const u8 dat                 ---- data length, exclude itself.
* Output         : None
* Return         : bool
                   True  ---- Information received.
                   False ---- No information received.
*******************************************************************************/
static bool ProgMCUDataIndication(tTransceiverFSM *receiver, const u8 dat)
{
    receiver->Timeout = 20;

    switch(receiver->status)
    {
    case RECEIVE_ACK:
        if(dat == PROGMCU_ACK)
        {
            AckFlag = TRUE;
        }
        else if(dat == PROGMCU_NACK)
        {
            NackFlag = TRUE;
        }
        receiver->Timeout = 0;
        break;

    case RECEIVE_DATA_LENGTH:
        receiver->FrameLen = dat;
        receiver->status = RECEIVE_DATA_CONTEXT;
        receiver->ptr = 0;
        break;

    case RECEIVE_DATA_CONTEXT:
        receiver->buf[receiver->ptr++] = dat;

        if(receiver->FrameLen-- == 0)
        {
            receiver->Timeout = 0;
            receiver->status = RECEIVE_ACK;
            return TRUE;
        }
        break;

    default:
        receiver->Timeout = 0;
        receiver->status  = RECEIVE_ACK;
        break;
    }

    return FALSE;
}

/*******************************************************************************
* Function Name  : ProgMCUUsartInterrupt
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ProgMCUUsartInterrupt(void)
{
    u8 tmpchar;

    //if(USART_GetITStatus(PROG_USART, USART_IT_RXNE) != RESET)
    if((PROG_USART->SR & (1UL << 5)) != 0)
    {
        //tmpchar = USART_ReceiveData(PROG_USART);
        tmpchar = (u8)PROG_USART->DR;

        if(!UsartState)
        {
            if(TRUE == ProgMCUDataIndication(&ProgMCUReceive, tmpchar))
            {
                InfoFlag = TRUE;
            }
        }
        else
        {
            if(TRUE == AssistDataIndication(&ProgMCUReceive, tmpchar))
            {
                AssistRspFlag = TRUE;
            }
        }
    }

    //if(USART_GetITStatus(PROG_USART, USART_IT_TXE) != RESET)
    if((PROG_USART->SR & (1UL << 7)) != 0)
    {
        if(ProgMCUTransmit.FrameLen != 0)
        {
            ProgMCUTransmit.FrameLen--;
            ProgMCUTransmit.Timeout = 20;
            //USART_SendData(PROG_USART, ProgMCUTransmit.buf[ProgMCUTransmit.ptr++]);
            PROG_USART->DR = ProgMCUTransmit.buf[ProgMCUTransmit.ptr++];
        }
        else
        {
            ProgMCUTransmit.status  = FALSE;
            ProgMCUTransmit.Timeout = 0;
            //USART_ITConfig(PROG_USART, USART_IT_TXE, DISABLE);
            PROG_USART->CR1 &= ~(1UL << 7);
        }
    }
}

