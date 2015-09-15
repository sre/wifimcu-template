#include "LED.h"
#include "GPIO.h"
#include "RCC.h"

void InitialiseLEDs()
{
	EnableLEDPeripheralClock();

	SetGPIOOutputMode(LEDGPIO,LEDMask<<FirstLEDPin);
	SetGPIOPushPullOutput(LEDGPIO,LEDMask<<FirstLEDPin);
	SetGPIOSpeed50MHz(LEDGPIO,LEDMask<<FirstLEDPin);
	SetGPIOPullUpResistor(LEDGPIO,LEDMask<<FirstLEDPin);
}

