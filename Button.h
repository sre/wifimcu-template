#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "stm32f4xx.h"

#include <stdbool.h>

void InitialiseUserButton();
void EnableUserButtonInterrupt();

#if defined(STM32F4DISCOVERY) || defined(STM32F429IDISCOVERY)

static inline bool UserButtonState()
{
	return (GPIOA->IDR>>0)&0x01;
}

#elif defined(NUCLEO_F446RE)

static inline bool UserButtonState()
{
	return !((GPIOC->IDR>>13)&0x01);
}

#elif defined(WIFIMCU)

static inline bool UserButtonState()
{
	return !((GPIOB->IDR>>2)&0x01);
}

#else

#error This board is not supported.

#endif

#endif
