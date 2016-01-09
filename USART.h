#ifndef __USART_H__
#define __USART_H__

#include "stm32f4xx.h"
#include "RCC.h"

static inline void SetUSARTBaudRate(USART_TypeDef *usart,uint32_t baudrate)
{
	uint32_t clock;
	if(usart==USART1 || usart==USART6) clock=PCLK2Frequency();
	else clock=PCLK1Frequency();

	uint32_t divider=(clock+baudrate/2)/baudrate;

	if(usart->CR1&USART_CR1_OVER8)
	{
		usart->BRR=((divider&~0x7)<<1)|(divider&0x7);
	}
	else
	{
		usart->BRR=divider;
	}
}

static inline void SendUSARTByte(USART_TypeDef *usart,uint8_t byte)
{
	while(!(usart->SR&USART_SR_TXE));
	usart->DR=byte;
}

static inline uint8_t ReceiveUSARTByte(USART_TypeDef *usart)
{
	while(!(usart->SR&USART_SR_RXNE));
	return usart->DR;
}

#endif
