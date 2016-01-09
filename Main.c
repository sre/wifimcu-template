#include "System.h"
#include "LED.h"
#include "Button.h"
#include "Printf.h"
#include "RCC.h"

#include <string.h>
#include <FreeRTOS.h>
#include <task.h>

static void LEDTask(void *parameters);
static void ButtonTask(void *parameters);

int main()
{
	InitialiseSystem();

	InitialisePrintf();
	printf("FreeRTOS LED blinker\n");
	printf("====================\n");
	printf("\n");

	printf("Initialising %d LED%s.\n",NumberOfLEDs,NumberOfLEDs==1?"":"s");
	InitialiseLEDs();

	printf("Initialising user buttons.\n");
	InitialiseUserButton();

	printf("Creating tasks.\n");
	xTaskCreate(LEDTask,"LEDTask",1024,NULL,2,NULL);
	xTaskCreate(ButtonTask,"ButtonTask",1024,NULL,1,NULL);

	printf("Starting scheduler.\n");
	vTaskStartScheduler();
}

static void LEDTask(void *parameters)
{
	printf("LED task startup.\n");

	uint32_t t=0;
	for(;;)
	{
		#if NumberOfLEDs>1
		SetLEDs(1<<(t%NumberOfLEDs));
		#else
		SetLEDs((t|((t>>1)&(t>>2)))&1);
		#endif

		vTaskDelay(160);
		t++;
	}	
}

static void ButtonTask(void *parameters)
{
	printf("Button task startup.\n");

	for(;;)
	{
		if(UserButtonState()) SetLEDs((1<<NumberOfLEDs)-1);
	}	
}
