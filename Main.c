#include "System.h"
#include "LED.h"
#include "Button.h"
#include "RCC.h"

#include <string.h>

void Delay(uint32_t time);

int main()
{
	InitialiseSystem();
	SysTick_Config(HCLKFrequency()/100);

	InitialiseLEDs();
	InitialiseUserButton();

	uint32_t t=0;
	for(;;)
	{
		#if NumberOfLEDs>1
		if(UserButtonState()) SetLEDs(0x0f);
		else SetLEDs(1<<((t>>4)%NumberOfLEDs));
		#else
		if(UserButtonState()) SetLEDs(0x01);
		else SetLEDs((t>>4)|((t>>5)&(t>>6))&1);
		#endif

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
