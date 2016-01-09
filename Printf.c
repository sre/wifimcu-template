#include "USART.h"
#include "GPIO.h"
#include "FormatString.h"

#if defined(WIFIMCU)

#define PrintfUSART USART2
#define PrintfBaudRate 115200
#define EnablePrintfUSARTClock() EnableAPB1PeripheralClock(RCC_APB1ENR_USART2EN)

#endif

#ifdef PrintfUSART

void InitialisePrintf()
{
	EnableAHB1PeripheralClock(RCC_AHB1ENR_GPIOAEN);

	SetGPIOAlternateFunctionMode(GPIOA,(1<<2)|(1<<3));
	SelectAlternateFunctionForGPIOPin(GPIOA,2,7);
	SelectAlternateFunctionForGPIOPin(GPIOA,3,7);
	SetGPIOPushPullOutput(GPIOA,(1<<2)|(1<<3));
	SetGPIOSpeed50MHz(GPIOA,(1<<2)|(1<<3));
	SetGPIOPullUpResistor(GPIOA,(1<<2)|(1<<3));

	EnablePrintfUSARTClock();

	PrintfUSART->CR1=USART_CR1_UE|USART_CR1_TE;
	PrintfUSART->CR2=0;
	PrintfUSART->CR3=0;
	SetUSARTBaudRate(PrintfUSART,PrintfBaudRate);
}

static void PrintfOutputFunction(char c,void *context)
{
	SendUSARTByte(PrintfUSART,c);
}

#else

void InitialisePrintf() {}

static void PrintfOutputFunction(char c,void *context) {}

#endif

int printf(const char *format,...)
{
	va_list args;
	va_start(args,format);
	int val=FormatString(PrintfOutputFunction,0,format,args);
	va_end(args);
	return val;
}

int puts(const char *s)
{
	int i=0;
	while(s[i]) PrintfOutputFunction(s[i++],0);
	PrintfOutputFunction('\n',0);

	return i+1;
}

int putchar(int c)
{
	PrintfOutputFunction(c,0);

	return 0;
}
