/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ict_communication.h"
#include "main.h"
#include "misc.h"
#include "m25pxx.h"
#include "progmcu.h"
#include "dwn_assist.h"
#include "md5.h"
#include "event.h"
#include "firmware.h"
#include <string.h>
/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    u8  Image_Name[8];      // 8 Bytes
    u8  Image_Version[2];   // 2 Bytes
    u8  Image_Length[4];    // 4 Bytes, little endian
    u8  Total_Packet[4];    // 2 Bytes, little endian
    u8  Download_Time[8];   // 8 Bytes
    u8  MD5_Hash_1[16];     // 16 Bytes
    u8  MD5_Hash_2[16];     // 16 Bytes
} ImageStruct;

/* Private define ------------------------------------------------------------*/
#define STC                 0x5A
#define ESC                 0x5C

#define BID_SCB             0x00
#define BID_MPB             0x01
#define BID_EPB             0x02
#define BID_PPB             0x03

#define BID                 BID_MPB
#define ICT_COMMU_UART      UART4

#define PIN_BUS_REQ         GPIO_Pin_9
#define PORT_BUS_REQ        GPIOC

#define PIN_BUS_DETECT      GPIO_Pin_8
#define PORT_BUS_DETECT     GPIOA

/* Private macro -------------------------------------------------------------*/
#define SEARCH_PAGE         55
#define CHECKING            99
#define ERASE_FLAG          0x55AA

#define ICT_UART_SPEED      115200UL
#define SECTOR_NUM          16
/* Private variables ---------------------------------------------------------*/
tTransceiverFSM ICT_CommuTransmit;
tTransceiverFSM ICT_CommuReceive;

u16 SectorToBeErase[SECTOR_NUM] = {0};
u16 EraseStatus;
u32 CurSector;

u32 CalculateMd5Flag = MD5_CALCULATED;
u32 CalculateCRCFlag = CRC_CALCULATED;

u32 ImageAddress;
u32 ImageSize;
u16 TotalPkt;
u8  Md5Value[16];
u8  Md5Buf[1024];

MD5_CTX ImageMd5;


u8 Version[17] = "Main0001_BootFFFF";




/* Private function prototypes -----------------------------------------------*/
static bool ICT_DataIndication(tTransceiverFSM *receiver, u8 dat);
static bool ICT_RS232BusDetect(void);
static void ICT_RS232BusRequest(bool state);
static u8   ICT_CommuCalChksum(const u8 *dat, u16 len);
static u32  ICT_ReadImage(u8 *buf, u16 pkt, u8 ImageName);
static bool ICT_WriteImage(u8 *buf, u16 pkt, u8 ImageName);
void ICT_CommuUsartInterrupt(void);

/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
* Function Name  : ICT_CommunicationInit
* Description    : Init program MCU usart.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ICT_CommuInit(void)
{
    NVIC_InitTypeDef  NVIC_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* Enable UART4 clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

    /* Enable the UART4 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQChannel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 15;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    /* Configure UART4 Tx (PC.10) as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed= GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* Configure UART4 Rx (PC.11) as input floating */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* RS232 Bus detect */
    GPIO_InitStructure.GPIO_Pin  = PIN_BUS_DETECT;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(PORT_BUS_DETECT, &GPIO_InitStructure);

    /* RS232 Bus request */
    GPIO_InitStructure.GPIO_Pin  = PIN_BUS_REQ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed= GPIO_Speed_50MHz;
    GPIO_Init(PORT_BUS_REQ, &GPIO_InitStructure);

    ICT_RS232BusRequest(FALSE);

    /* UART4 configuration ----------------------------------------------------*/
    /* UART4 configured as follow:
        - BaudRate = ICT_UART_SPEED baud
        - Word Length = 8 Bits
        - One Stop Bit
        - EVEN parity
        - Receive and transmit enabled
        - Hardware flow control disabled (RTS and CTS signals)
    */
    USART_InitStructure.USART_BaudRate = ICT_UART_SPEED;
    USART_InitStructure.USART_WordLength = USART_WordLength_9b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_Even;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    USART_Init(ICT_COMMU_UART, &USART_InitStructure);
    USART_Cmd(ICT_COMMU_UART, ENABLE);
    USART_ITConfig(ICT_COMMU_UART, USART_IT_RXNE, ENABLE);
}

/*******************************************************************************
* Function Name  : ICT_DataIndication
* Description    : Indicate if there is any frame have been received.
* Input          : receiver     the receiver finite state machine
                   dat          the input data
* Output         : None
* Return         : bool         TRUE,   there is a frame has been received
                                FALSE,  there is no frame has been received.
*******************************************************************************/
#define RCV_IDLE        0x00
#define RCV_BID         0x01
#define RCV_LEN         0x02
#define RCV_CTXT        0x03

static bool ICT_DataIndication(tTransceiverFSM *receiver, u8 dat)
{
    static bool EscapeFlag = FALSE;
    bool flag = FALSE;

    if(gEventFlag.Bits.MsgProc)
    {
        goto RECEIVE_END;
    }

    receiver->Timeout = 20;
    receiver->FrameChksum += dat;

    switch(receiver->status)
    {
    case RCV_IDLE:
        if(dat == STC)
        {
            receiver->status = RCV_BID;
            receiver->FrameChksum = 0;
        }
        break;

    case RCV_BID:
        if(dat == BID)
        {
            receiver->status = RCV_LEN;
        }
        else
        {
            goto RECEIVE_END;
        }
        break;

    case RCV_LEN:
        if(EscapeFlag)
        {
            EscapeFlag = FALSE;
            dat ^= ESC;
        }
        else if(dat == ESC)
        {
            EscapeFlag = TRUE;
            break;
        }

        receiver->FrameLen = dat;
        receiver->buf[0] = dat;
        receiver->ptr = 1;
        receiver->status = RCV_CTXT;

        break;

    case RCV_CTXT:
        if(EscapeFlag)
        {
            EscapeFlag = FALSE;
            dat ^= ESC;
        }
        else if(dat == ESC)
        {
            EscapeFlag = TRUE;
            break;
        }

        receiver->buf[receiver->ptr++] = dat;

        if(--receiver->FrameLen == 0)
        {
            if(receiver->FrameChksum == 0)
            {
                flag = TRUE;
            }
            receiver->Timeout = 0;
            receiver->status = RCV_IDLE;
        }

        break;

    default:
RECEIVE_END:
        receiver->Timeout = 0;
        receiver->status  = RCV_IDLE;
        break;
    }

    return flag;
}

/*******************************************************************************
* Function Name  : ICT_DataRequest
* Description    : Request to send data.
* Input          : const u8 *src
                   src[0]       ---- data length, exclude itself.
                   src[1...]    ---- data string.
* Output         : None
* Return         : bool   TRUE  ---- Request sucess
                          FALSE ---- Request fails
*******************************************************************************/
bool ICT_DataRequest(const u8 *src)
{
    u8 *des;
    u16 length;

    if(ICT_CommuTransmit.status == TRUE)
    {
        return FALSE;
    }

    if(ICT_RS232BusDetect())
    {
        ICT_RS232BusRequest(TRUE);
    }
    else
    {
        return FALSE;
    }

    length = src[0] + 1;

    ICT_CommuTransmit.buf[0] = STC;
    ICT_CommuTransmit.buf[1] = BID;

    // Fill the LT field.
    if((length == STC) || (length == ESC))
    {
        ICT_CommuTransmit.buf[2] = ESC;
        ICT_CommuTransmit.buf[3] = length ^ ESC;
        ICT_CommuTransmit.FrameLen = 4;
        des = &ICT_CommuTransmit.buf[4];
    }
    else
    {
        ICT_CommuTransmit.buf[2] = length;
        ICT_CommuTransmit.FrameLen = 3;
        des = &ICT_CommuTransmit.buf[3];
    }

    length = src[0];

    while(length != 0)
    {
        src++;
        length--;

        if((*src == STC) || (*src == ESC))
        {
            *des++ = ESC;
            *des++ = *src ^ ESC;
            ICT_CommuTransmit.FrameLen++;
            ICT_CommuTransmit.FrameLen++;
        }
        else
        {
            ICT_CommuTransmit.FrameLen++;
            *des++ = *src;
        }
    }

    ICT_CommuTransmit.FrameChksum = ICT_CommuCalChksum(&ICT_CommuTransmit.buf[1], ICT_CommuTransmit.FrameLen - 1);

    if((ICT_CommuTransmit.FrameChksum == STC) || (ICT_CommuTransmit.FrameChksum == ESC))
    {
        *des++ = ESC;
        *des++ = ICT_CommuTransmit.FrameChksum - ESC;
        ICT_CommuTransmit.FrameLen++;
        ICT_CommuTransmit.FrameLen++;
    }
    else
    {
        *des++ = ICT_CommuTransmit.FrameChksum;
        ICT_CommuTransmit.FrameLen++;
    }

    // start to send
    DisAllInterrupt();
    ICT_CommuTransmit.status = TRUE;
    ICT_CommuTransmit.ptr = 0;
    ICT_CommuTransmit.Timeout = 20;
    EnAllInterrupt();

    // send the data
    USART_ITConfig(ICT_COMMU_UART, USART_IT_TXE, ENABLE);

    return TRUE;
}

/*******************************************************************************
* Function Name  : ICT_RS232BusDetect
* Description    : Detect if the rs232 bus is available.
* Input          : None
* Output         : None
* Return         : bool     TRUE, the bus is available
                            FALSE, the bus is not available
*******************************************************************************/
static bool ICT_RS232BusDetect(void)
{
    if(GPIO_ReadInputDataBit(PORT_BUS_DETECT, PIN_BUS_DETECT) == Bit_RESET)
    {
        MCUDelayMs(5);

        if(GPIO_ReadInputDataBit(PORT_BUS_DETECT, PIN_BUS_DETECT) == Bit_RESET)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*******************************************************************************
* Function Name  : ICT_RS232BusDetect
* Description    : Detect if the rs232 bus is available.
* Input          : bool     TRUE,  request to hold the RS232 bus
                            FALSE, request to release the RS232 bus
* Output         : None
* Return         : None
*******************************************************************************/
static void ICT_RS232BusRequest(bool state)
{
    if(state != 0)
    {
        GPIO_WriteBit(PORT_BUS_REQ, PIN_BUS_REQ, Bit_RESET);
    }
    else
    {
        GPIO_WriteBit(PORT_BUS_REQ, PIN_BUS_REQ, Bit_SET);
    }
}

/*******************************************************************************
* Function Name  : ICT_CommuCalChksum
* Description    : Calculate the checksum.
* Input          : None
* Output         : None
* Return         : u8, the checksum.
*******************************************************************************/
static u8 ICT_CommuCalChksum(const u8 *dat, u16 len)
{
    u8  sum = 0;

    while(len-- != 0)
    {
        sum -= *dat++;
    }

    return sum;
}

/*******************************************************************************
* Function Name  : ICT_CommuTimeout
* Description    : ICT communication time out counter.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ICT_CommuTimeout(void)
{
    if(ICT_CommuTransmit.Timeout)
    {
        ICT_CommuTransmit.Timeout--;

        if(0 == ICT_CommuTransmit.Timeout)
        {
            ICT_CommuTransmit.status = FALSE;
            ICT_RS232BusRequest(FALSE);
            USART_ITConfig(ICT_COMMU_UART, USART_IT_TXE, DISABLE);
        }
    }

    if(ICT_CommuReceive.Timeout)
    {
        ICT_CommuReceive.Timeout--;

        if(0 == ICT_CommuReceive.Timeout)
        {
            ICT_CommuReceive.status = RCV_IDLE;
            USART_ITConfig(ICT_COMMU_UART, USART_IT_RXNE, ENABLE);
        }
    }
}

/*******************************************************************************
* Function Name  : ICT_MsgProcess
* Description    : ICT communication message process.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
#define MSG_ID_OFFSET   1

void ICT_MsgProcess(void)
{
    u8 RspBuf[300];
    u32 i;
    RspBuf[0] = 0;

    switch(ICT_CommuReceive.buf[MSG_ID_OFFSET])
    {
    case START_ERASE_MCU_REQ:
        gEventFlag.Bits.EraseMcu = TRUE;
        RspBuf[0] = 1;
        RspBuf[1] = START_ERASE_MCU_RSP;
        break;

    case START_PROGRAM_MCU_REQ:
        gEventFlag.Bits.ProgMcu = TRUE;
        RspBuf[0] = 1;
        RspBuf[1] = START_PROGRAM_MCU_RSP;
        break;

    case START_VERIFY_MCU_REQ:
        gEventFlag.Bits.VerifyMcu = TRUE;
        RspBuf[0] = 1;
        RspBuf[1] = START_VERIFY_MCU_RSP;
        break;

    case CAL_RTC_REQ:
        //DMA_Reconfig();
        gEventFlag.Bits.CalRTC = TRUE;
        //MeasureRTCFlag = TRUE;
        RspBuf[0] = 1;
        RspBuf[1] = CAL_RTC_RSP;
        break;

    case START_AUTO_PROGRAM_MCU_REQ:
        gEventFlag.Bits.AutoProgMcu = TRUE;
        RspBuf[0] = 1;
        RspBuf[1] = START_AUTO_PROGRAM_MCU_RSP;
        break;

    case HOLD_MCU_RESET_REQ:
        HOLD_MCU_RESET();
        RspBuf[0] = 1;
        RspBuf[1] = HOLD_MCU_RESET_RSP;
        break;

    case RELEASE_MCU_RESET_REQ:
        RELEASE_MCU_RESET();
        RspBuf[0] = 1;
        RspBuf[1] = RELEASE_MCU_RESET_RSP;
        break;

    case READ_MCU_FLASH_REQ:
        {
        #if 0
            u32 pkt = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1] + ((u16)ICT_CommuReceive.buf[MSG_ID_OFFSET + 2] * 256);

            if(pkt == 0)
            {
                ProgMCUStart();
            }

            RspBuf[0] = 131;
            RspBuf[1] = READ_MCU_FLASH_RSP;
            RspBuf[2] = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1];
            RspBuf[3] = ICT_CommuReceive.buf[MSG_ID_OFFSET + 2];

            ProgMCUReadMemory(pkt * 128 + 0x08000000, &RspBuf[4], 128);
        #endif
        }
        break;

    case WRITE_MCU_FLASH_REQ:
        break;

    case READ_METER_IMAGE_REQ:
        {
            u16 pkt = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1] + ((u16)ICT_CommuReceive.buf[MSG_ID_OFFSET + 2] * 256);

            RspBuf[0] = ICT_ReadImage(&RspBuf[4], pkt, METER_IMAGE) + 3;
            RspBuf[1] = READ_METER_IMAGE_RSP;
            RspBuf[2] = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1];
            RspBuf[3] = ICT_CommuReceive.buf[MSG_ID_OFFSET + 2];
        }
        break;

    case WRITE_METER_IMAGE_REQ:
        {
            u16 pkt = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1] + ((u16)ICT_CommuReceive.buf[MSG_ID_OFFSET + 2] * 256);

            RspBuf[0] = 2;
            RspBuf[1] = WRITE_METER_IMAGE_RSP;
            if(ICT_WriteImage(&ICT_CommuReceive.buf[MSG_ID_OFFSET + 3], pkt, METER_IMAGE) == TRUE)
            {
                RspBuf[2] = 0;
            }
            else
            {
                RspBuf[2] = 1;
            }
        }
        break;

    case READ_ASSIT_IMAGE_REQ:
        {
            u16 pkt = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1] + ((u16)ICT_CommuReceive.buf[MSG_ID_OFFSET + 2] * 256);

            RspBuf[0] = ICT_ReadImage(&RspBuf[4], pkt, ASSIST_IMAGE) + 3;
            RspBuf[1] = READ_ASSIT_IMAGE_RSP;
            RspBuf[2] = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1];
            RspBuf[3] = ICT_CommuReceive.buf[MSG_ID_OFFSET + 2];
        }
        break;

    case WRITE_ASSIT_IMAGE_REQ:
        {
            u16 pkt = ICT_CommuReceive.buf[MSG_ID_OFFSET + 1] + ((u16)ICT_CommuReceive.buf[MSG_ID_OFFSET + 2] * 256);

            RspBuf[0] = 2;
            RspBuf[1] = WRITE_ASSIT_IMAGE_RSP;
            if(ICT_WriteImage(&ICT_CommuReceive.buf[MSG_ID_OFFSET + 3], pkt, ASSIST_IMAGE) == TRUE)
            {
                RspBuf[2] = 0;
            }
            else
            {
                RspBuf[2] = 1;
            }
        }
        break;

    case START_ERASE_IMAGE_REQ:

        if(ICT_CommuReceive.buf[MSG_ID_OFFSET + 1] == 0x00)
        {
          for(i = 0 ; i < (SECTOR_NUM / 2) ; i++)
            SectorToBeErase[i] = ERASE_FLAG;
        }
        else if(ICT_CommuReceive.buf[MSG_ID_OFFSET + 1] == 0x01)
        {
          for(i = (SECTOR_NUM / 2)  ; i < ((SECTOR_NUM / 2) + 1) ; i++)
            SectorToBeErase[i] = ERASE_FLAG;        
            
        }
        else
        {
            break;
        }

        EraseStatus = SEARCH_PAGE;
        RspBuf[0] = 1;
        RspBuf[1] = START_ERASE_IMAGE_RSP;
        break;

    case READ_VERSION_REQ:
        RspBuf[0] = 18;
        RspBuf[1] = READ_VERSION_RSP;
        M25_ReadBytes(ASSPROG_BASE_ADDRESS+0x24+12, &Version[13], 8);
        memcpy(&RspBuf[2], Version, sizeof(Version));
        break;
        
    default:
        break;
    }

    if(RspBuf[0] != 0)
    {
        ICT_DataRequest(RspBuf);
    }
}

/*******************************************************************************
* Function Name  : ICT_ReadImage
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static u32 ICT_ReadImage(u8 *buf, u16 pkt, u8 ImageName)
{
    u32 length;
    u32 address;

    if(ImageName == ASSIST_IMAGE)
    {
        address = ASSIMAGE_BASE_ADDRESS + pkt * PACKET_SIZE;
    }
    else if(ImageName == METER_IMAGE)
    {
        address = MTRIMAGE_BASE_ADDRESS + pkt * PACKET_SIZE;
    }
    else
    {
        DPRINTF("\r\nReadImage Function error!!!");
        return FALSE;
    }

    if(pkt == 0)
    {
        length = 40;
        M25_ReadBytes(address, buf, 56);
        memcpy(&buf[24], &buf[40], 16);
    }
    else
    {
        length = PACKET_SIZE;
        M25_ReadBytes(address, buf, PACKET_SIZE);
    }

    return length;
}

/*******************************************************************************
* Function Name  : ICT_GetImageLength
* Description    : Get the length of the image.
* Input          : u8 ImageName --- The image identifier.
* Output         : None
* Return         : None
*******************************************************************************/
u32 ICT_GetImageLength(u8 ImageName)
{
    u32 length;
    u32 address;

    if(ImageName == ASSIST_IMAGE)
    {
        address = ASSIMAGE_BASE_ADDRESS + 10;
    }
    else if(ImageName == METER_IMAGE)
    {
        address = MTRIMAGE_BASE_ADDRESS + 10;
    }
    else
    {
        DPRINTF("\r\nReadImage Length Function error!!!");
        return FALSE;
    }

    M25_ReadBytes(address, &length, 4);

//    if(ImageName == METER_IMAGE)
//    {
//        if(length % PACKET_SIZE)
//        {
//            length = length + 128 - (length % PACKET_SIZE);
//        }
//    }

    return length;
}

/*******************************************************************************
* Function Name  : ICT_GetImageMd5
* Description    : Get the length of the image.
* Input          : u8 ImageName --- The image identifier.
* Output         : None
* Return         : None
*******************************************************************************/
bool ICT_GetImageMd5(u8 *md5, u8 ImageName)
{
    u32 address;

    if(ImageName == ASSIST_IMAGE)
    {
        address = ASSIMAGE_BASE_ADDRESS + 24;
    }
    else if(ImageName == METER_IMAGE)
    {
        address = MTRIMAGE_BASE_ADDRESS + 24;
    }
    else
    {
        DPRINTF("\r\nReadImage MD5 Function error!!!");
        return FALSE;
    }

    return M25_ReadBytes(address, md5, 16);
}

/*******************************************************************************
* Function Name  : ICT_WriteImage
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
/*
    the packet 0x0000 format

    Image Name      --  8  Bytes
    Image Version   --  2  Bytes
    Image Length    --  4  Bytes
    Total Packet    --  2  Bytes
    Download Time   --  8  Bytes
    MD5 Hash 1      --  16 Bytes
    MD5 Hash 2      --  16 Bytes
*/
typedef struct{
    u8      name[8];
    u8      ver[2];
    u8      length[4];
    u8      packet[2];
    u8      time[8];
    u8      hash[16];
    u8      Res[88];
}tImageInfo;
tImageInfo  AssImageInfo={0};
tImageInfo  MtrImageInfo;

static bool ICT_WriteImage(u8 *buf, u16 pkt, u8 ImageName)
{
    bool flag;
    u32 address;
    u8  info[128];
    if(ImageName == ASSIST_IMAGE)
    {
        address = ASSIMAGE_BASE_ADDRESS + pkt * PACKET_SIZE;
    }
    else if(ImageName == METER_IMAGE)
    {
        address = MTRIMAGE_BASE_ADDRESS + pkt * PACKET_SIZE;
    }
    else
    {
        DPRINTF("\r\nWriteImage Function error!!!");
        return FALSE;
    }

    if(pkt == 0)
    {
        flag = M25_WriteBytes(address, buf, 40);

        ImageAddress = address + PACKET_SIZE;
        ImageSize = (u32)buf[10] + (u32)buf[11] * 256 + (u32)buf[12] * 256 * 256 + (u32)buf[13] * 256 * 256 * 256;
        TotalPkt  = (u16)buf[14] + (u16)buf[15] * 256;
    }
    else
    {
        flag = M25_WriteBytes(address, buf, PACKET_SIZE);

        if(pkt == TotalPkt)
        {
            MD5Init(&ImageMd5);
            CalculateMd5Flag = MD5_CALCULATING;

            if(ImageName == ASSIST_IMAGE)
            {
                M25_ReadBytes(ASSIMAGE_BASE_ADDRESS, &AssImageInfo, PACKET_SIZE);
                TargetInfo.bootloader_size = ICT_GetImageLength(ASSIST_IMAGE);//ряж╙
            }
            else if(ImageName == METER_IMAGE)
            {
                M25_ReadBytes(MTRIMAGE_BASE_ADDRESS, &MtrImageInfo, PACKET_SIZE);
            }    
        }
    }
    

    
    if(flag == FALSE)
    {
        DPRINTF("\r\nProgramming the EPB flash error!!! Packet number: %d.", pkt);
    }

    return flag;
}

/*******************************************************************************
* Function Name  : ICT_CalculateImageMD5
* Description    : Erase the flash of the image.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ICT_CalculateImageMD5(void)
{
  if(CalculateMd5Flag == MD5_CALCULATING)
  {
    if(ImageSize > sizeof(Md5Buf))
    {
        M25_ReadBytes(ImageAddress, Md5Buf, sizeof(Md5Buf));
        MD5Update(&ImageMd5, Md5Buf, sizeof(Md5Buf));
        ImageSize -= sizeof(Md5Buf);
        ImageAddress += sizeof(Md5Buf);
    }
    else
    {
        M25_ReadBytes(ImageAddress, Md5Buf, ImageSize);
        MD5Update(&ImageMd5, Md5Buf, ImageSize);

        MD5Final(Md5Value, &ImageMd5);

        if(!M25_WriteBytes(((ImageAddress > ASSIMAGE_BASE_ADDRESS) ? (ASSIMAGE_BASE_ADDRESS + 40) : (MTRIMAGE_BASE_ADDRESS + 40)), Md5Value, 16))
        {
            DPRINTF("\r\n Writing MD5 to flash error!!!");
        }
        CalculateMd5Flag = MD5_CALCULATED;
        //
        
        CalculateCRCFlag = CRC_CALCULATING;
        ImageSize = ICT_GetImageLength((ImageAddress > ASSIMAGE_BASE_ADDRESS) ? (ASSIST_IMAGE) : (METER_IMAGE));
        ImageAddress = (ImageAddress > ASSIMAGE_BASE_ADDRESS) ? (ASSPROG_BASE_ADDRESS) : (MTRPROG_BASE_ADDRESS);
    }
  }
}
/*******************************************************************************
* Function Name  : ICT_CalculateImageCRC
* Description    : Erase the flash of the image.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
u16 crcnum = 0;
void ICT_CalculateImageCRC(void)
{
  u8 zero[2] = { 0, 0 };

  if(CalculateCRCFlag == CRC_CALCULATING)
  {
    if(ImageSize > sizeof(Md5Buf))
    {
      M25_ReadBytes(ImageAddress , Md5Buf, sizeof(Md5Buf));

      if(ImageSize == ICT_GetImageLength(METER_IMAGE))
      {
        crcnum = Crc16_helper(Md5Buf, sizeof(Md5Buf), 0);
      }
      else
      {
        crcnum = Crc16_helper(Md5Buf, sizeof(Md5Buf), crcnum);
      }
      
      ImageSize -= sizeof(Md5Buf);
      ImageAddress += sizeof(Md5Buf);
    }
    else
    {
      M25_ReadBytes(ImageAddress , Md5Buf, sizeof(Md5Buf));
      crcnum = Crc16_helper(Md5Buf, ImageSize, crcnum);
      crcnum = Crc16_helper(zero,2,crcnum);
      zero[0] = crcnum;
      zero[1] = crcnum >> 8;
      M25_WriteBytes((ImageAddress > ASSIMAGE_BASE_ADDRESS) ? (ASSIMAGE_BASE_ADDRESS + 56) : (MTRIMAGE_BASE_ADDRESS + 56),zero,2);
      crcnum = 0;
      ImageSize = ICT_GetImageLength(METER_IMAGE);
      ImageAddress = 128;
      CalculateCRCFlag = CRC_CALCULATED;
      crcnum = 0;
    }
  }
}
/*******************************************************************************
* Function Name  : ICT_EraseImage
* Description    : Erase the flash of the image.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ICT_EraseImage(void)
{
    u16 i;

    switch(EraseStatus)
    {
    case SEARCH_PAGE:
        for(i = 0; i < sizeof(SectorToBeErase); i++)
        {
            if(SectorToBeErase[i] == ERASE_FLAG)
            {
                CurSector = i;
                EraseStatus = CHECKING;

                if(!M25_SectorErase(CurSector * SECTOR_SIZE))
                {
                    memset(SectorToBeErase, 0x00, sizeof(SectorToBeErase));
                    EraseStatus = 0;
                    CurSector = 0;
                    ICT_ProgResultReq(ERASE_IMAGE_RESULT_REQ, 0x01);
                }
                return;
            }
        }

        ICT_ProgResultReq(ERASE_IMAGE_RESULT_REQ, 0x00);
        EraseStatus = 0;

        break;

    case CHECKING:
        if(!M25_IsProgramming())
        {
            SectorToBeErase[CurSector] = 0;
            CurSector = 0;
            EraseStatus = SEARCH_PAGE;
        }
        break;

    default:
        break;
    }
}

/*******************************************************************************
* Function Name  : ICT_ProgResultReq
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ICT_ProgResultReq(u8 Command, u8 Result)
{
    u8 buf[16];

    buf[0] = 0;

    switch(Command)
    {
    case ERASE_MCU_RESULT_REQ:
    case PROGRAM_MCU_RESULT_REQ:
    case VERIFY_MCU_RESULT_REQ:
    case AUTO_PROGRAM_MCU_RESULT_REQ:
    case ERASE_IMAGE_RESULT_REQ:
    case CAL_RTC_REQ:
        buf[0] = 2;
        buf[1] = Command;
        buf[2] = Result;
        break;

    default:
        DPRINTF("\r\nUnknown commands wants to issue certain result.");
        break;
    }

    if(buf[0] != 0)
    {
        ICT_DataRequest(buf);
    }
}

/*******************************************************************************
* Function Name  : ICT_CommuUsartInterrupt
* Description    : Erase mcu flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ICT_CommuUsartInterrupt(void)
{
    u8 tmpchar;

    if(USART_GetITStatus(ICT_COMMU_UART, USART_IT_RXNE) != RESET)
    {
        tmpchar = USART_ReceiveData(ICT_COMMU_UART);

        if(ICT_DataIndication(&ICT_CommuReceive, tmpchar))
        {
            gEventFlag.Bits.MsgProc = TRUE;
        }
    }

    if(USART_GetITStatus(ICT_COMMU_UART, USART_IT_TXE) != RESET)
    {
        if(ICT_CommuTransmit.FrameLen != 0)
        {
            ICT_CommuTransmit.FrameLen--;
            ICT_CommuTransmit.Timeout = 20;
            USART_SendData(ICT_COMMU_UART, ICT_CommuTransmit.buf[ICT_CommuTransmit.ptr++]);
        }
        else
        {
            ICT_CommuTransmit.status  = FALSE;
            ICT_CommuTransmit.Timeout = 0;
            ICT_RS232BusRequest(FALSE);
            USART_ITConfig(ICT_COMMU_UART, USART_IT_TXE, DISABLE);
        }
    }
}


