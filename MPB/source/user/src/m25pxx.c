/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "m25pxx.h"
#include "misc.h"

/* Private typedef -----------------------------------------------------------*/
typedef enum
{
    M25_WREN = 0x06,        // write enable
    M25_WRDI = 0x04,        // write disable
    M25_RDID = 0x9F,        // read identification
    M25_RDSR = 0x05,        // read status register
    M25_WRSR = 0x01,        // write status register
    M25_READ = 0x03,        // read data bytes
    M25_FAST_READ = 0x0B,   // read data bytes in higher speed
    M25_PP   = 0x02,        // page program
    M25_SE   = 0xD8,        // sector erase
    M25_BE   = 0xC7,        // bulk erase
    M25_DP   = 0xB9         // deep power down
} m25pxx_command;


/* Private define ------------------------------------------------------------*/
#define PIN_CS                  GPIO_Pin_12
#define PORT_CS                 GPIOB

#define PIN_SCK                 GPIO_Pin_13
#define PIN_MISO                GPIO_Pin_14
#define PIN_MOSI                GPIO_Pin_15
#define SPI_PORT                GPIOB
#define SPI_NUM                 SPI2

#define PIN_WP                  GPIO_Pin_12
#define PORT_WP                 GPIOA

#define DUMMY_BYTE              0xA5

#define M25PXX_PAGE_SIZE        256
#define M25PXX_WRITE_TIMEOUT    20

/* Private macro -------------------------------------------------------------*/
#define RESET_M25_CS()          GPIO_ResetBits(PORT_CS, PIN_CS)
#define SET_M25_CS()            GPIO_SetBits(PORT_CS, PIN_CS)

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static bool M25_WriteEnable(void);
//static void M25_WriteDisable(void);
static void M25_WaitForWriteEnd(void);
static bool M25_IsHwProtected(void);
static bool M25_WritePage(u32 addr, const void *dat, u32 length);
static u8   M25_SendByte(u8 byte);
/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
* Function Name  : DataFlashInit
* Description    : Init data flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void DataFlashInit(void)
{
    SPI_InitTypeDef  SPI_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    /* GPIOA, AFIO, SPIx Periph clock enable */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* Configure SPIx pins: SCK, MISO and MOSI ---------------------------------*/
    GPIO_InitStructure.GPIO_Pin   = PIN_SCK | PIN_MISO | PIN_MOSI;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;

    GPIO_Init(SPI_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = PIN_CS;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;

    GPIO_Init(PORT_CS, &GPIO_InitStructure);
    GPIO_SetBits(PORT_CS, PIN_CS);

    // wp pin init
    GPIO_InitStructure.GPIO_Pin   = PIN_WP;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;

    GPIO_Init(PORT_WP, &GPIO_InitStructure);

    /* SPIx configuration ------------------------------------------------------*/
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS  = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI_NUM, &SPI_InitStructure);

    /* Disable SPIx CRC calculation */
    SPI_CalculateCRC(SPI_NUM, DISABLE);

    /* Enable SPIx */
    SPI_Cmd(SPI_NUM, ENABLE);
}

/*******************************************************************************
* Function Name  : SPI_FLASH_Test
* Description    : Test data flash
* Input          : None
* Output         : None
* Return         : TRUE, Flash test success.
                   FALSE, Flash test fails.
*******************************************************************************/
#define FLASH_SectorToErase 0x000000
#define FLASH_TEST_ADDRESS  0x0000F4

bool DataFlashTest(void)
{
    u8 TxBuffer[] = "Hello, world. Testing the data flash.";
    u8 RxBuffer[sizeof(TxBuffer)];

    /* Get SPI Flash ID */
    M25_ReadId(RxBuffer);

    /* Check the SPI Flash ID 0x208013 is for M25PE40 */
    if((RxBuffer[0] != 0x20) || (RxBuffer[1] != 0x80) || (RxBuffer[2] != 0x13))
    {
        return FALSE;
    }
    else
    {
        /* Erase SPI FLASH Sector to write on */
        if(!M25_SectorErase(FLASH_SectorToErase))
        {
            return FALSE;
        }

        /* Write TxBuffer data to SPI FLASH memory */
        M25_WriteBytes(FLASH_TEST_ADDRESS, TxBuffer, sizeof(TxBuffer));

        /* Read data from SPI FLASH memory */
        M25_ReadBytes(FLASH_TEST_ADDRESS, RxBuffer, sizeof(TxBuffer));

        /* Check the corectness of written dada */
        if(memcmp(RxBuffer, TxBuffer, sizeof(TxBuffer)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_WriteEnable
* Description    : Enable data flash write.
* Input          : None
* Output         : None
* Return         : bool     TRUE    Enable write flash success
                            FALSE   Enable write flash fail
*******************************************************************************/
static bool M25_WriteEnable(void)
{
    if(M25_IsHwProtected())
    {
        DPRINTF("\r\nThe data flash is hardware protect. Please check the jumper.");
        return FALSE;
    }

    RESET_M25_CS();
    M25_SendByte(M25_WREN);
    SET_M25_CS();

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_WriteDisable
* Description    : Disable data flash write.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
/*static void M25_WriteDisable(void)
{
    RESET_M25_CS();
    SPI_SendByte(M25_WRDI);
    SET_M25_CS();
}*/

/*******************************************************************************
* Function Name  : M25_ReadId
* Description    : Read data flash manufacture ID.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void M25_ReadId(u8 *dat)
{
    RESET_M25_CS();
    M25_SendByte(M25_RDID);

    for(u32 i = 0; i < 3; i++)
    {
        dat[i] = M25_SendByte(DUMMY_BYTE);
    }

    SET_M25_CS();
}

/*******************************************************************************
* Function Name  : M25_ReadStatus
* Description    : Read data flash status.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
u8 M25_ReadStatus(void)
{
    u8 status = 0;
    RESET_M25_CS();

    M25_SendByte(M25_RDSR);

    status = M25_SendByte(DUMMY_BYTE);

    SET_M25_CS();

    return status;
}

/*******************************************************************************
* Function Name  : M25_WriteStatus
* Description    : Write data flash status.
* Input          : None
* Output         : None
* Return         : bool     TRUE  - write status done
                            FALSE - write status failed
*******************************************************************************/
bool M25_WriteStatus(u8 status)
{
    if(!M25_WriteEnable())
    {
        return FALSE;
    }

    RESET_M25_CS();

    M25_SendByte(M25_WRSR);

    M25_SendByte(status);

    SET_M25_CS();

    M25_WaitForWriteEnd();

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_ReadBytes
* Description    : read data from data flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
bool M25_ReadBytes(u32 addr, void *dat, u32 length)
{
    u8 *p = (u8 *)dat;

    assert_param(addr < 0x1000000);

    RESET_M25_CS();

    for(vu32 i = 0; i < 100; i++);

    M25_SendByte(M25_READ);

    M25_SendByte(addr  >> 16);
    M25_SendByte((addr >> 8) & 0xFF);
    M25_SendByte(addr & 0xFF);

    while(length--)
    {
        *p = M25_SendByte(DUMMY_BYTE);
        p++;
    }

    SET_M25_CS();

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_WritePage
* Description    : write data to flash using page program instruction.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static bool M25_WritePage(u32 addr, const void *dat, u32 length)
{
    u8 *p = (u8 *)dat;

    assert_param(addr < 0x1000000);

    if(!M25_WriteEnable())
    {
        return FALSE;
    }

    RESET_M25_CS();

    M25_SendByte(M25_PP);

    M25_SendByte(addr  >> 16);
    M25_SendByte((addr >> 8) & 0xFF);
    M25_SendByte(addr & 0xFF);

    while(length--)
    {
        M25_SendByte(*p);
        p++;
    }

    SET_M25_CS();

    M25_WaitForWriteEnd();

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_WriteBytes
* Description    : write data to flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
bool M25_WriteBytes(u32 addr, const void *dat, u32 length)
{
    u8 tmp[M25PXX_PAGE_SIZE];
    u8 *src = (u8 *)dat;
    u32 BlockLength;

    assert_param(addr < 0x1000000);

    do
    {
        if(((addr % M25PXX_PAGE_SIZE) + length) > M25PXX_PAGE_SIZE)
        {
            BlockLength = M25PXX_PAGE_SIZE - (addr % M25PXX_PAGE_SIZE);
            length -= BlockLength;
        }
        else
        {
            BlockLength = length;
            length = 0;
        }

        if(!M25_WritePage(addr, src, BlockLength))
        {
            return FALSE;
        }

        if(!M25_ReadBytes(addr, tmp, BlockLength))
        {
            return FALSE;
        }

        if(memcmp(tmp, src, BlockLength))
        {
            return FALSE;
        }

        src = src + BlockLength;
        addr += BlockLength;

    } while(length > 0);

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_SectorErase
* Description    : Erase flash sector.
* Input          : addr the start address of the flash page
* Output         : None
* Return         : bool FALSE, erase fail, TRUE, erase done.
*******************************************************************************/
bool M25_SectorErase(u32 addr)
{
    assert_param(addr < 0x1000000);

    if(!M25_WriteEnable())
    {
        return FALSE;
    }

    RESET_M25_CS();
    M25_SendByte(M25_SE);

    M25_SendByte(addr  >> 16);
    M25_SendByte((addr >> 8) & 0xFF);
    M25_SendByte(addr & 0xFF);

    SET_M25_CS();

    //M25_WaitForWriteEnd();

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_WholeChipErase
* Description    : Erase the whole flash chip.
* Input          : None
* Output         : None
* Return         : bool     TRUE  - write status done
                            FALSE - write status failed
*******************************************************************************/
bool M25_WholeChipErase(void)
{
    u8 status = M25_ReadStatus();

    if(status & (M25_BP0 | M25_BP1 | M25_SRWD))
    {
        status &= ~(M25_BP0 | M25_BP1 | M25_SRWD);

        DPRINTF("\r\nWarnning, the flash is software protected!!!");

        if(!M25_WriteStatus(status))
        {
            return FALSE;
        }
    }

    if(!M25_WriteEnable())
    {
        return FALSE;
    }

    RESET_M25_CS();
    M25_SendByte(M25_BE);
    SET_M25_CS();

    //M25_WaitForWriteEnd();

    return TRUE;
}

/*******************************************************************************
* Function Name  : M25_WaitForWriteEnd
* Description    : Wait for the end of writing SPI flash.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void M25_WaitForWriteEnd(void)
{
    u8 status1 = 0;
    RESET_M25_CS();

    M25_SendByte(M25_RDSR);

    do
    {
        status1 = M25_SendByte(DUMMY_BYTE);
    }
    while(status1 & M25_WIP);

    SET_M25_CS();
}

/*******************************************************************************
* Function Name  : M25_IsProgramming
* Description    : Check if the data flash is in programming state.
* Input          : None
* Output         : None
* Return         : bool     TRUE    -- in the programming state.
                            FALSE   -- not in the programming state.
*******************************************************************************/
bool M25_IsProgramming(void)
{
    if(M25_ReadStatus() & M25_WIP)
    {
        return TRUE;
    }

    return FALSE;
}

/*******************************************************************************
* Function Name  : M25_IsHwProtected
* Description    : Check if the chip is hardware protected.
* Input          : None
* Output         : None
* Return         : bool     TRUE, the chip is hardware protected.
                            FALSE, the chip is not hardware protected.
*******************************************************************************/
static bool M25_IsHwProtected(void)
{
    if(GPIO_ReadInputDataBit(PORT_WP, PIN_WP) == Bit_RESET)
    {
        MCUDelayMs(2);

        if(GPIO_ReadInputDataBit(PORT_WP, PIN_WP) == Bit_RESET)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*******************************************************************************
* Function Name  : M25_SendByte
* Description    : Send or receive data through SPI.
* Input          : byte to be sent
* Output         : None
* Return         : u8   received data
*******************************************************************************/
static u8 M25_SendByte(u8 byte)
{
    /* Loop while DR register in not emplty */
    //while (SPI_I2S_GetFlagStatus(SPI_NUM, SPI_I2S_FLAG_TXE) == RESET);
    while(!(SPI_NUM->SR & SPI_I2S_FLAG_TXE));

    /* Send byte through the SPIx peripheral */
    //SPI_I2S_SendData(SPI_NUM, byte);
    SPI_NUM->DR = byte;

    /* Wait to receive a byte */
    //while (SPI_I2S_GetFlagStatus(SPI_NUM, SPI_I2S_FLAG_RXNE) == RESET);
    while(!(SPI_NUM->SR & SPI_I2S_FLAG_RXNE));

    /* Return the byte read from the SPI bus */
    //return SPI_I2S_ReceiveData(SPI_NUM);
    return SPI_NUM->DR;
}

