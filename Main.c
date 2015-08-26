#include "System.h"
#include "LED.h"
#include "Button.h"
#include "RCC.h"

#include <string.h>

void Delay(uint32_t time);

int main()
{
	InitializeSystem();
	SysTick_Config(HCLKFrequency()/100);

	InitializeLEDs();
	InitializeUserButton();

	uint32_t t=0;
	for(;;)
	{
		if(UserButtonState()) SetLEDs(0x0f);
		else SetLEDs(1<<((t>>4)&3));

		t++;
		Delay(1);
	}	
}

volatile uint32_t SysTickCounter=0;

void Delay(uint32_t time)
{
	uint32_t end=SysTickCounter+time;
	while(SysTickCounter!=end);
}

void SysTick_Handler()
{  
	SysTickCounter++;
}
