#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "stm32f4xx.h"

#include <stdbool.h>

void InitializeUserButton();
void EnableUserButtonInterrupt();

#if defined(STM32F407xx) || defined(STM32F429xx) // Assume this means STM32F4DISCOVERY or 32F429IDISCOVERY

static inline bool UserButtonState()
{
	return (GPIOA->IDR>>0)&0x01;
}

#elif defined(STM32F446xx) // Assume this means NUCLEO-F446RE

static inline bool UserButtonState()
{
	return !((GPIOC->IDR>>13)&0x01);
}

#else

#error This board is not supported.

#endif

#endif
