/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
//#include "core_cm3.h"
#include "typedef.h"
#include "main.h"
#include "event.h"
#include "m25pxx.h"
#include "misc.h"
#include "progmcu.h"
#include "ict_communication.h"
#include "dwn_assist.h"
#include "swd_dap.h"
#include "swd_pin.h"
#include "misc.h"
#include "md5.h"
#include "target.h"

/* Private typedef -----------------------------------------------------------*/
typedef void (* EVENT_PROCESS)(void);
typedef u16 eventflgtype;

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#define POWER_FAIL()    FALSE

/* Private variables ---------------------------------------------------------*/
const EVENT_PROCESS gFunctionsList[] =
{
    ICT_MsgProcess,
    AutoProgMCUEvent,
    EraseMCUEvent,
    ProgramMCUEvent,
    VerifyMCUEvent,
    SecondEvent,
};

EventStruct gEventFlag;
/* Private function prototypes -----------------------------------------------*/
void LoopCheck(void);
void Idle(void);
void ClearEventFlag(eventflgtype tmp);

/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
* Function Name  : main
* Description    : Main program
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
int main(void)
{
#ifdef DEBUG
    debug();
#endif
	SystemInit();           // system initialization
    PowerOnSelfTest();		// power on self test
    FEED_DOG();             // feed dog
    
	while(!POWER_FAIL())
    {
        u16 i;
        eventflgtype event_flag;

        event_flag = 1;

        for (i = 0; i < sizeof(gFunctionsList) / sizeof(EVENT_PROCESS); i++)
        {
            // scan the event flag
            if (gEventFlag.Value & event_flag)
            {
                ClearEventFlag(event_flag);
                (*gFunctionsList[i])();
                break;      // exit the for loop in order to insure the high PRI event implement first
            }

            event_flag = event_flag << 1;
        }

		LoopCheck();

        if(i >= sizeof(gFunctionsList) / sizeof(EVENT_PROCESS))
        {
            Idle();
        }
    }
}

/*******************************************************************************
* Function Name  : ClearEventFlag
* Description    : Configures the different system clocks.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ClearEventFlag(eventflgtype tmp)
{
    DisAllInterrupt();
    gEventFlag.Value &= ~tmp;
    EnAllInterrupt();
}

/*******************************************************************************
* Function Name  : LoopCheck
* Description    : Check the system after every time.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void LoopCheck(void)
{
  if(FeedDog)
  {
    FeedDog = FALSE;
    FEED_DOG();
  }
}

/*******************************************************************************
* Function Name  : Idle
* Description    : Idle.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void Idle(void)
{
  ICT_EraseImage();
  ICT_CalculateImageMD5();//À„MD5
  ICT_CalculateImageCRC();//À„CRC

}

#ifdef  DEBUG
/*******************************************************************************
* Function Name  : assert_failed
* Description    : Reports the name of the source file and the source line number
*                  where the assert_param error has occurred.
* Input          : - file: pointer to the source file name
*                  - line: assert_param error line source number
* Output         : None
* Return         : None
*******************************************************************************/
void assert_failed(u8* file, u32 line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  //("\r\nThe source file %s, line %d, parameter error!!!", file, line);

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

