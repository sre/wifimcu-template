#include "System.h"
#include "LED.h"
#include "Button.h"
#include "RCC.h"

#include <string.h>
#include <FreeRTOS.h>
#include <task.h>

static void LEDTask(void *parameters);

int main()
{
	InitialiseSystem();

	InitialiseLEDs();
	InitialiseUserButton();

	xTaskCreate(LEDTask,"LEDTask",1024,NULL,2,NULL);

	vTaskStartScheduler();
}

static void LEDTask(void *parameters)
{
	uint32_t t=0;
	for(;;)
	{
		#if NumberOfLEDs>1
		if(UserButtonState()) SetLEDs(0x0f);
		else SetLEDs(1<<(t%NumberOfLEDs));
		#else
		if(UserButtonState()) SetLEDs(0x01);
		else SetLEDs(t|((t>>1)&(t>>2))&1);
		#endif

		vTaskDelay(160);
		t++;
	}	
}
