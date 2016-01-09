#ifndef __LED_H__
#define __LED_H__

#include "stm32f4xx.h"
#include "GPIO.h"

#if defined(STM32F4DISCOVERY)

#define NumberOfLEDs 4
#define LEDGPIO GPIOD
#define FirstLEDPin 12
#define EnableLEDPeripheralClock() EnableAHB1PeripheralClock(RCC_AHB1ENR_GPIODEN)

#elif defined(STM32F429IDISCOVERY)

#define LEDGPIO GPIOG
#define NumberOfLEDs 2
#define FirstLEDPin 13
#define EnableLEDPeripheralClock() EnableAHB1PeripheralClock(RCC_AHB1ENR_GPIOGEN)

#elif defined(NUCLEO_F446RE)

#define LEDGPIO GPIOA
#define NumberOfLEDs 1
#define FirstLEDPin 5
#define EnableLEDPeripheralClock() EnableAHB1PeripheralClock(RCC_AHB1ENR_GPIOAEN)

#elif defined(WIFIMCU)

#define LEDGPIO GPIOA
#define NumberOfLEDs 1
#define FirstLEDPin 4
#define EnableLEDPeripheralClock() EnableAHB1PeripheralClock(RCC_AHB1ENR_GPIOAEN)
#define LEDsAreInverted

#else

#error This board is not supported.

#endif

#define LEDMask ((1<<NumberOfLEDs)-1)

void InitialiseLEDs();

static inline void SetLEDs(int leds)
{
	uint16_t val=LEDGPIO->ODR;
	val&=~(LEDMask<<FirstLEDPin);
	#ifdef LEDsAreInverted
	leds^=LEDMask;
	#endif
	val|=leds<<FirstLEDPin;
	LEDGPIO->ODR=val;
}

static inline void ToggleLEDs(int leds)
{
	ToggleGPIOPins(LEDGPIO,(leds&LEDMask)<<FirstLEDPin);
}

static inline void TurnOnLEDs(int leds)
{
	SetGPIOPinsHigh(LEDGPIO,(leds&LEDMask)<<FirstLEDPin);
}

static inline void TurnOffLEDs(int leds)
{
	SetGPIOPinsLow(LEDGPIO,(leds&LEDMask)<<FirstLEDPin);
}

#endif
