#include "swd_pin.h"

void SWD_PinInit(void)
{   
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    /* release trst pin */
    SWDIO_SET_OUTPUT();
    SWCLK_SET_OUTPUT();
    SWDIO_SET();
    SWCLK_CLR();
}

