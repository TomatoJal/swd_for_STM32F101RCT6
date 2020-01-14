/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "misc.h"
#include "ict_communication.h"
#include "dwn_assist.h"

/* Private typedef -----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/
#define CTRL_CHAR1              0x7E
#define CTRL_CHAR2              0x7D

#define ASSIST_USART            USART1
#define ASSIST_USART_SPEED      1000000UL

#define CMD_OFFSET              1
#define DATA_OFFSET             2
#define IDX_OFFSET              2
#define PROGRAM_DATA_OFFSET     4

/* Private macro -------------------------------------------------------------*/
#define ToU32L(ptr)  ((u32)((u8 *)ptr)[0] + ((u32)((u8 *)ptr)[1] << 8) + ((u32)((u8 *)ptr)[2] << 16) + ((u32)((u8 *)ptr)[3] << 24))
#define ToU16L(ptr)  ((u32)((u8 *)ptr)[0] + ((u32)((u8 *)ptr)[1] << 8))

/* Private variables ---------------------------------------------------------*/
extern tTransceiverFSM ProgMCUTransmit;
extern tTransceiverFSM ProgMCUReceive;
extern bool UsartState;
static vu16 AssistWaitCnt;
vu16 AssistRspFlag;

/* Private function prototypes -----------------------------------------------*/
static void AssistDataRequest(const u8 *src);
static u8   AssistCalChksum(const u8 *src, u16 length);
static void AssistSetCounter(u16 cnt);
static u16  AssistGetCounter(void);
static bool AssistWaitForResponce(u8 *rsp, u8 cmd, u16 timeout, u8 *err_code);
static bool AssistWaitForResqust(u8 cmd, u16 timeout, u8 *err_code);

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : AssistDataRequest
* Description    : transmit the message through Uart
* Input          : src      the pointer to sending data.
* Output         : None
* Return         : None
*******************************************************************************/
void AssistInit(void)
{
    // init the USART
    USART_InitTypeDef USART_InitStructure;
//    NVIC_InitTypeDef  NVIC_InitStructure;
//    GPIO_InitTypeDef  GPIO_InitStructure;
    //USART_DeInit(ASSIST_USART);
    USART_Cmd(ASSIST_USART, DISABLE);

    /* USART1 configuration ----------------------------------------------------*/
    /* USART1 configured as follow:
        - BaudRate = 1000000 baud
        - Word Length = 8 Bits
        - One Stop Bit
        - EVEN parity
        - Receive and transmit enabled
        - Hardware flow control disabled (RTS and CTS signals)
    */
    USART_InitStructure.USART_BaudRate = 1000000;//ASSIST_USART_SPEED;
    USART_InitStructure.USART_WordLength = USART_WordLength_9b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_Even;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    USART_Init(ASSIST_USART, &USART_InitStructure);
    USART_Cmd(ASSIST_USART, ENABLE);
    USART_ITConfig(ASSIST_USART, USART_IT_RXNE, ENABLE);

    DPRINTF("\r\nThe usart to MCU speed is %d bps.", ASSIST_USART_SPEED);



    UsartState = TRUE;
}

/*******************************************************************************
* Function Name  : AssistDataIndication
* Description    : Indicate if there is any frame have been received.
* Input          : receiver     the receiver finite state machine
                   dat          the input data
* Output         : None
* Return         : bool         TRUE,   there is a frame has been received
                                FALSE,  there is no frame has been received.
*******************************************************************************/
#define ASSIST_RCV_IDLE     0x00
#define ASSIST_RCV_LEN      0x01
#define ASSIST_RCV_CTXT     0x02

bool AssistDataIndication(tTransceiverFSM *receiver, u8 dat)
{
    static bool EscapeFlag = FALSE;
    bool flag = FALSE;

    receiver->Timeout = 20;

    switch(receiver->status)
    {
        case ASSIST_RCV_IDLE:
            if(dat == CTRL_CHAR1)
            {
                receiver->status = ASSIST_RCV_LEN;
            }
            break;

        case ASSIST_RCV_LEN:
            if(EscapeFlag)
            {
                EscapeFlag = FALSE;
                dat ^= 0x20;
            }
            else if(dat == CTRL_CHAR2)
            {
                EscapeFlag = TRUE;
                break;
            }

            receiver->FrameLen = dat;
            receiver->FrameChksum = dat;
            receiver->buf[0] = dat;
            receiver->ptr = 1;
            receiver->status = ASSIST_RCV_CTXT;

            break;

        case ASSIST_RCV_CTXT:
            if(EscapeFlag)
            {
                EscapeFlag = FALSE;
                dat ^= 0x20;
            }
            else if(dat == CTRL_CHAR2)
            {
                EscapeFlag = TRUE;
                break;
            }

            if(receiver->FrameLen-- != 0)
            {
                receiver->FrameChksum ^= dat;
                receiver->buf[receiver->ptr++] = dat;
            }
            else
            {
                // compare the chksum
                if(receiver->FrameChksum == dat)
                {
                    flag = TRUE;
                }
                /*else
                {
                    DPRINTF("\r\nChecksum error!!!");
                }*/
                receiver->Timeout = 0;
                receiver->status  = ASSIST_RCV_IDLE;
            }

            break;

        default:
            receiver->Timeout = 0;
            receiver->status  = ASSIST_RCV_IDLE;
            break;
    }

    return flag;

}

/*******************************************************************************
* Function Name  : AssistDataRequest
* Description    : transmit the message through Uart
* Input          : src      the pointer to sending data.
* Output         : None
* Return         : None
*******************************************************************************/
static void AssistDataRequest(const u8 *src)
{
    u8 *des = &ProgMCUTransmit.buf[1];
    u16 length = src[0] + 1;

    ProgMCUTransmit.buf[0] = CTRL_CHAR1;
    ProgMCUTransmit.FrameLen = 1;
    ProgMCUTransmit.FrameChksum = AssistCalChksum(src, src[0] + 1);

    for( ;length > 0; length--)
    {
        if((*src == CTRL_CHAR1) || (*src == CTRL_CHAR2))
        {
            *des++ = CTRL_CHAR2;
            *des++ = *src ^ 0x20;
            ProgMCUTransmit.FrameLen++;
            ProgMCUTransmit.FrameLen++;
        }
        else
        {
            ProgMCUTransmit.FrameLen++;
            *des++ = *src;
        }
        src++;
    }

    // add the check sum for this packet
    if((ProgMCUTransmit.FrameChksum == CTRL_CHAR1) || ((ProgMCUTransmit.FrameChksum == CTRL_CHAR2)))
    {
        *des++ = CTRL_CHAR2;
        *des = ProgMCUTransmit.FrameChksum ^ 0x20;
        ProgMCUTransmit.FrameLen++;
        ProgMCUTransmit.FrameLen++;
    }
    else
    {
        *des = ProgMCUTransmit.FrameChksum;
        ProgMCUTransmit.FrameLen++;
    }

    // start to send
    DisAllInterrupt();
    ProgMCUTransmit.status = TRUE;
    ProgMCUTransmit.ptr = 0;
    ProgMCUTransmit.Timeout = 20;
    EnAllInterrupt();

    /* This function uses very frequently. */
    //USART_ITConfig(ASSIST_USART, USART_IT_TXE, ENABLE);
    ASSIST_USART->CR1 |= (1UL << 7);
}

/*******************************************************************************
* Function Name  : AssistWriteFlash
* Description    : Write a packet of data to the MCU flash.
* Input          : dat:     pointer to the content.
                   pkts:    packet number.
* Output         : None
* Return         : u8       TRUE, write to MCU flash sucess.
                            FALASE, write to MCU flash failed.
*******************************************************************************/
bool AssistWriteFlash(const u8 *dat, u16 pkts, u8 *err_code)
{
    u8 tmpbuf[PACKET_SIZE + 4];

    tmpbuf[0] = 3 + PACKET_SIZE;
    tmpbuf[1] = CMD_WRITE_REQ;
    tmpbuf[2] = pkts % 256;
    tmpbuf[3] = pkts / 256;

    memcpy(&tmpbuf[4], dat, PACKET_SIZE);

    AssistDataRequest(tmpbuf);

    return AssistWaitForResponce(tmpbuf, CMD_WRITE_REQ, 20, err_code);
}

/*******************************************************************************
* Function Name  : AssistReadFlash
* Description    : Read a packet of data from the MCU flash.
* Input          : dat:     pointer to the content.
                   pkts:    packet number.
* Output         : None
* Return         : bool     TRUE, write to MCU flash sucess.
                            FALASE, write to MCU flash failed.
*******************************************************************************/
bool AssistReadFlash(u8 *dat, u16 pkts, u8 *err_code)
{
    dat[0] = 3;
    dat[1] = CMD_READ_REQ;
    dat[2] = pkts % 256;
    dat[3] = pkts / 256;

    AssistDataRequest(dat);

    return AssistWaitForResponce(dat, CMD_READ_REQ, 20, err_code);
}

/*******************************************************************************
* Function Name  : AssistQuery
* Description    : Read a packet of data from the MCU flash.
* Input          : dat:     pointer to the content.
                   pkts:    packet number.
* Output         : None
* Return         : bool     TRUE, write to MCU flash sucess.
                            FALASE, write to MCU flash failed.
*******************************************************************************/
bool AssistQueryMD5(u8 *err_code, u8 *md5, u32 *flag)
{
    u8 dat[32];

    dat[0] = 1;
    dat[1] = CMD_QUERY_MD5_REQ;

    AssistDataRequest(dat);

    if(!AssistWaitForResponce(dat, CMD_QUERY_MD5_REQ, 20, err_code))
    {
        DPRINTF("\r\nAssit query command has failed! Please check!");
        return FALSE;
    }

    memcpy(flag, dat, 4);
    memcpy(md5, &dat[4], 16);

    switch(*flag)
    {
    case MD5_CALCULATING:
        DPRINTF("\r\nMD5 is calculating!");
        return FALSE;
    case MD5_CALCULATED:
        return TRUE;
    default:
        DPRINTF("\r\nMD5 is not calculate at all!!!");
        return FALSE;
    }

    //return TRUE;
}

/*******************************************************************************
* Function Name  : AssistTest
* Description    : Read a packet of data from the MCU flash.
* Input          : dat:     pointer to the content.
                   pkts:    packet number.
* Output         : None
* Return         : bool     TRUE, write to MCU flash sucess.
                            FALASE, write to MCU flash failed.
*******************************************************************************/
bool AssistTest(void)
{
    u8 dat[2];

    dat[0] = 1;
    dat[1] = CMD_TEST_REQ;

    AssistDataRequest(dat);

    FEED_DOG();
    if(!AssistWaitForResponce(dat, CMD_TEST_REQ, 400, &dat[1]))
    {
        if(ASSIST_RSP_TIMEOUT == dat[1])
        {
            DPRINTF("\r\nAssit test command is time out!");
        }
        else
        {
            DPRINTF("\r\nAssit test command has failed!");
        }

        return FALSE;
    }

    DPRINTF("\r\nAssit test command has done!");

    if(dat[0] & 0x01)
    {
        DPRINTF("\r\nUUP LSE can't start up!!!");
    }
    else
    {
        DPRINTF("\r\nUUP LSE can starts up!!!");
    }

    if(dat[0] & 0x02)
    {
        DPRINTF("\r\nUUP HSE can't start up!!! Using HSI!!!");
    }
    else
    {
        DPRINTF("\r\nUUP HSE can starts up!!! Using HSE!!!");
    }

    return TRUE;
}

/*******************************************************************************
* Function Name  : AssistTest
* Description    : Read a packet of data from the MCU flash.
* Input          : dat:     pointer to the content.
                   pkts:    packet number.
* Output         : None
* Return         : bool     TRUE, write to MCU flash sucess.
                            FALASE, write to MCU flash failed.
*******************************************************************************/
/*
bool AssistWaitTest(void)
{
    u8 dat[2];

    FEED_DOG();

    if(!AssistWaitForResqust(CMD_TEST_REQ, 400, dat))
    {
        DPRINTF("\r\nAssit wait for test command has failed! Please check!");
        return FALSE;
    }

    dat[0] = 1;
    dat[1] = CMD_TEST_RSP;

    AssistDataRequest(dat);
    DPRINTF("\r\nAssit wait for test command has done!");

    return TRUE;
}
*/

/*******************************************************************************
* Function Name  : AssistWaitForBufEmpty
* Description    : Read a packet of data from the MCU flash.
* Input          : dat:     pointer to the content.
                   pkts:    packet number.
* Output         : None
* Return         : bool     TRUE, write to MCU flash sucess.
                            FALASE, write to MCU flash failed.
*******************************************************************************/
bool AssistWaitForBufEmpty(void)
{
    u8 dat[2];

    FEED_DOG();

    if(!AssistWaitForResqust(CMD_BUF_EMPTY_REQ, 200, dat))
    {
        DPRINTF("\r\nAssit wait for buffer empty command has failed! Please check!");
        return FALSE;
    }

    DPRINTF("\r\nAssit wait for buffer empty command has done!");
    return TRUE;
}

/*******************************************************************************
* Function Name  : AssistCalChksum
* Description    : Calculate the check sum for the packet.
* Input          : src:     pointer to the content.
                   len:     length of the content.
* Output         : None
* Return         : Return the checksum of the content.
*******************************************************************************/
static u8 AssistCalChksum(const u8 *src, u16 length)
{
    u8 chksum = 0;

    while(length--)
    {
        chksum ^= *src;
        src++;
    }

    return chksum;
}

/*******************************************************************************
* Function Name  : AssistSetCounter
* Description    : Set the counter for responce delay.
* Input          : cnt      the new counter value.
* Output         : None
* Return         : None
*******************************************************************************/
static void AssistSetCounter(u16 cnt)
{
    DisAllInterrupt();
    AssistWaitCnt = cnt;
    EnAllInterrupt();
}

/*******************************************************************************
* Function Name  : AssistSetCounter
* Description    : Get the counter for responce delay.
* Input          : None
* Output         : None
* Return         : u16      the counter value.
*******************************************************************************/
static u16 AssistGetCounter(void)
{
    return AssistWaitCnt;
}

/*******************************************************************************
* Function Name  : AssistTimeout
* Description    : Get the counter for responce delay.
* Input          : None
* Output         : None
* Return         : u16      the counter value.
*******************************************************************************/
void AssistTimeout(void)
{
    if(AssistWaitCnt)
    {
        AssistWaitCnt--;
    }
}

/*******************************************************************************
* Function Name  : AssistWaitForResqust
* Description    : Get the counter for responce delay.
* Input          : None
* Output         : None
* Return         : u16      the counter value.
*******************************************************************************/
static bool AssistWaitForResqust(u8 cmd, u16 timeout, u8 *err_code)
{
    bool flag = FALSE;

    AssistRspFlag = FALSE;
    *err_code = ASSIST_NO_ERROR;

    AssistSetCounter(timeout);

    while((!AssistRspFlag) && (AssistGetCounter()));

    if(!AssistRspFlag)
    {
        *err_code = ASSIST_RSP_TIMEOUT;
        return FALSE;
    }

    switch(cmd)
    {
    /*
    case CMD_TEST_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_TEST_REQ)
        {
            flag = TRUE;
        }
        else
        {
            *err_code = ASSIST_REQ_RSP_NOT_MATCH;
        }
        break;
    */
    case CMD_BUF_EMPTY_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_BUF_EMPTY_REQ)
        {
            flag = TRUE;
        }
        break;

    default:
        break;
    }

    return flag;
}

/*******************************************************************************
* Function Name  : AssistWaitForResponce
* Description    : Get the counter for responce delay.
* Input          : None
* Output         : None
* Return         : u16      the counter value.
*******************************************************************************/
/*
ProgMCUReceive.buf[BUF_LENGTH] structure:

ProgMCUReceive.buf[0]:      Length for the received packet.
ProgMCUReceive.buf[1]:      Command code.
ProgMCUReceive.buf[2...]:   Data
*/
enum
{
    ASSIST_ACK = 0x06,
    ASSIST_NAK = 0x15,
};

static bool AssistWaitForResponce(u8 *rsp, u8 cmd, u16 timeout, u8 *err_code)
{
    bool flag = FALSE;

    AssistRspFlag = FALSE;
    *err_code = ASSIST_NO_ERROR;

    AssistSetCounter(timeout);

    while((!AssistRspFlag) && (AssistGetCounter()));

    if(!AssistRspFlag)
    {
        *err_code = ASSIST_RSP_TIMEOUT;
        return FALSE;
    }

    switch(cmd)
    {
    case CMD_WRITE_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_WRITE_RSP)
        {
            if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_NAK)
            {
                *err_code = ProgMCUReceive.buf[3];
            }
            else if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_ACK)
            {
                flag = TRUE;
            }
        }
        else
        {
            *err_code = ASSIST_REQ_RSP_NOT_MATCH;
        }
        break;

    case CMD_BLANK_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_BLANK_RSP)
        {
            if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_NAK)
            {
                *err_code = ProgMCUReceive.buf[3];
            }
            else if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_ACK)
            {
                flag = TRUE;
            }
        }
        else
        {
            *err_code = ASSIST_REQ_RSP_NOT_MATCH;
        }
        break;

    case CMD_READ_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_READ_RSP)
        {
            if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_NAK)
            {
                *err_code = ProgMCUReceive.buf[3];
            }
            else if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_ACK)
            {
                flag = TRUE;
                memcpy(rsp, &ProgMCUReceive.buf[3], PACKET_SIZE + 2);
            }
        }
        else
        {
            *err_code = ASSIST_REQ_RSP_NOT_MATCH;
        }
        break;

    case CMD_TEST_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_TEST_RSP)
        {
            if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_ACK)
            {
                *rsp = ProgMCUReceive.buf[3];
                flag = TRUE;
            }
        }
        else
        {
            *err_code = ASSIST_REQ_RSP_NOT_MATCH;
        }
        break;

    case CMD_QUERY_MD5_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_QUERY_MD5_RSP)
        {
            /*if(ProgMCUReceive.buf[DATA_OFFSET] == ASSIST_ACK)
            {
                flag = TRUE;
            }*/
            memcpy(rsp, &ProgMCUReceive.buf[DATA_OFFSET], 20);
            flag = TRUE;
        }
        else
        {
            *err_code = ASSIST_REQ_RSP_NOT_MATCH;
        }
        break;

    case CMD_BUF_EMPTY_REQ:
        if(ProgMCUReceive.buf[CMD_OFFSET] == CMD_BUF_EMPTY_REQ)
        {
            flag = TRUE;
        }
        break;
    default:
        break;
    }
    return flag;
}

