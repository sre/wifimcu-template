#include "System.h"
#include "LED.h"
#include "Button.h"
#include "Printf.h"
#include "RCC.h"

#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <lwip/tcpip.h>

static void NetTask(void *parameters);
static void InitDone(void *argument);

int main()
{
	InitialiseSystem();

	InitialisePrintf();
	printf("FreeRTOS LwIP stack\n");
	printf("===================\n");
	printf("\n");

	InitialiseLEDs();
	InitialiseUserButton();

	printf("Creating task.\n");
	xTaskCreate(NetTask,"NetTask",1024,NULL,2,NULL);

	printf("Starting scheduler.\n");
	vTaskStartScheduler();
}

static void NetTask(void *parameters)
{
	printf("In networking task.\n");

	printf("Creating semaphore.\n");
	SemaphoreHandle_t semaphore=xSemaphoreCreateCounting(1,0);
	if(!semaphore)
	{
		printf("Could not create semaphore.\n");
		return;
	}

	printf("Initalising TCP/IP stack...\n");
	tcpip_init(InitDone,&semaphore);
	xSemaphoreTake(semaphore,portMAX_DELAY);
	vQueueDelete(semaphore);

	printf("Shutting down\n");

	vTaskDelete(NULL);
}

static void InitDone(void *argument)
{
	printf("TCP/IP ready.\n");
	SemaphoreHandle_t *semaphore=(xSemaphoreHandle *)argument;
	xSemaphoreGive(*semaphore);
}
