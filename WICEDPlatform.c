#include "GPIO.h"
#include "RCC.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "wwd_constants.h"
#include "platform/wwd_sdio_interface.h"
#include "platform/wwd_bus_interface.h"
#include "network/wwd_buffer_interface.h"
#include "network/wwd_network_constants.h"

#define wiced_assert(a,b)

#define ResetGPIO GPIOB
#define ResetPin 14
#define Bootstrap0GPIO GPIOB
#define Bootstrap0Pin 0
#define Bootstrap1GPIO GPIOB
#define Bootstrap1Pin 1
#define Clock32kGPIO GPIOA
#define Clock32kPin 0
//#define Clock32kPin 11

#define SDIOOutOfBandIRQGPIO GPIOA
#define SDIOOutOfBandIRQPin 0
#define SDIOClockGPIO GPIOB
#define SDIOClockPin 15
#define SDIOCommandGPIO GPIOA
#define SDIOCommandPin 6
#define SDIOData0GPIO GPIOB
#define SDIOData0Pin 7
#define SDIOData1GPIO GPIOA
#define SDIOData1Pin 8
#define SDIOData2GPIO GPIOA
#define SDIOData2Pin 9
#define SDIOData3GPIO GPIOB
#define SDIOData3Pin 5

static void InitialiseGPIOAsOutput(volatile GPIO_TypeDef *gpio,int pin)
{
	SetGPIOOutputMode(gpio,1<<pin);
	SetGPIOPushPullOutput(gpio,1<<pin);
	SetGPIOSpeed50MHz(gpio,1<<pin);
	SetGPIOPullUpResistor(gpio,1<<pin);
}

static void InitialiseGPIOAsInput(volatile GPIO_TypeDef *gpio,int pin)
{
	SetGPIOInputMode(gpio,1<<pin);
	SetGPIONoPullResistor(gpio,1<<pin);
}

static void InitialiseGPIOAsAlternateFunction(volatile GPIO_TypeDef *gpio,int pin,int function)
{
	SetGPIOAlternateFunctionMode(gpio,1<<pin);
	SelectAlternateFunctionForGPIOPin(gpio,pin,function);
	SetGPIOPushPullOutput(gpio,1<<pin);
	SetGPIOSpeed50MHz(gpio,1<<pin);
	SetGPIOPullUpResistor(gpio,1<<pin);
}

void host_platform_reset_wifi(wiced_bool_t reset_asserted);
void host_platform_power_wifi(wiced_bool_t power_enabled);
wwd_result_t host_platform_init_wlan_powersave_clock( void );
wwd_result_t host_platform_deinit_wlan_powersave_clock( void );

wwd_result_t host_platform_init()
{
	host_platform_deinit_wlan_powersave_clock();

	InitialiseGPIOAsOutput(ResetGPIO,ResetPin);
	host_platform_reset_wifi(true);	 /* Start wifi chip in reset */

	return WWD_SUCCESS;
}

wwd_result_t host_platform_deinit( void )
{
	InitialiseGPIOAsOutput(ResetGPIO,ResetPin);
	host_platform_reset_wifi(true);

	host_platform_deinit_wlan_powersave_clock( );

	return WWD_SUCCESS;
}

void host_platform_reset_wifi(wiced_bool_t reset_asserted)
{
	if(reset_asserted) SetGPIOPinsLow(ResetGPIO,1<<ResetPin);
	else SetGPIOPinsHigh(ResetGPIO,1<<ResetPin);
}

void host_platform_power_wifi(wiced_bool_t power_enabled)
{
}

wwd_result_t host_platform_init_wlan_powersave_clock( void )
{
	#if defined(WICED_USE_WIFI_32K_CLOCK_MCO)
	InitialiseGPIOAsAlternateFunction(Clock32kGPIO,Clock32kPin,GPIO_AF_MCO);
	SetGPIONoPullResistor(Clock32kGPIO,1<<Clock32kPin);

	RCC->CFGR&=~(RCC_CFGR_MCO1PRE|RCC_CFGR_MCO1);
	RCC->CFGR|=(RCC_CFGR_MCO1PRE_0*0)|(RCC_CFGR_MCO1_0*1);
	#endif

	return WWD_SUCCESS;
}

wwd_result_t host_platform_deinit_wlan_powersave_clock( void )
{
	InitialiseGPIOAsOutput(Clock32kGPIO,Clock32kPin);
	SetGPIOPinsLow(Clock32kGPIO,1<<Clock32kPin);
	return WWD_SUCCESS;
}




#define MaxSDIOEnumerationRetries 500
#define MaxNormalCommandWaitLoops 100000
#define MaxDMACommandWaitLoops 100000
#define MaxTransferRetries 5

#define SDIOAllErrorsMask (SDIO_STA_CCRCFAIL|SDIO_STA_DCRCFAIL|SDIO_STA_CTIMEOUT|SDIO_STA_DTIMEOUT|SDIO_STA_TXUNDERR|SDIO_STA_RXOVERR|SDIO_STA_STBITERR)

static uint8_t ReceiveBuffer[MAX(2*1024,WICED_LINK_MTU+64)];
static SemaphoreHandle_t SDIOTransferFinishedSemaphore;
static volatile bool SDIOTransferFailed;

wwd_result_t host_platform_bus_init(void)
{
	SDIOTransferFinishedSemaphore=xSemaphoreCreateCounting(0xffffffff,0);
	if(!SDIOTransferFinishedSemaphore) return WWD_SEMAPHORE_ERROR;

	EnableAHB1PeripheralClock(RCC_AHB1ENR_GPIOAEN);
	EnableAHB1PeripheralClock(RCC_AHB1ENR_GPIOBEN);
	EnableAHB1PeripheralClock(RCC_AHB1ENR_DMA2EN);
	EnableAPB2PeripheralClock(RCC_APB2ENR_SDIOEN);

	InitialiseGPIOAsOutput(Bootstrap0GPIO,Bootstrap0Pin);
	InitialiseGPIOAsOutput(Bootstrap1GPIO,Bootstrap1Pin);
	SetGPIOPinsLow(Bootstrap0GPIO,1<<Bootstrap0Pin);
	SetGPIOPinsLow(Bootstrap1GPIO,1<<Bootstrap1Pin);

	InitialiseGPIOAsAlternateFunction(SDIOClockGPIO,SDIOClockPin,12);
	InitialiseGPIOAsAlternateFunction(SDIOCommandGPIO,SDIOCommandPin,12);
	InitialiseGPIOAsAlternateFunction(SDIOData0GPIO,SDIOData0Pin,12);
	InitialiseGPIOAsAlternateFunction(SDIOData1GPIO,SDIOData1Pin,12);
	InitialiseGPIOAsAlternateFunction(SDIOData2GPIO,SDIOData2Pin,12);
	InitialiseGPIOAsAlternateFunction(SDIOData3GPIO,SDIOData3Pin,12);

	SetAPB2PeripheralReset(RCC_APB2RSTR_SDIORST);
	ClearAPB2PeripheralReset(RCC_APB2RSTR_SDIORST);

	int divisor=PCLK2Frequency()/400000-2;
	if(divisor>255) divisor=255;

	SDIO->POWER=3;
	SDIO->DCTRL=SDIO_DCTRL_RWMOD;
	SDIO->CLKCR=SDIO_CLKCR_PWRSAV|SDIO_CLKCR_CLKEN|divisor;

	SDIO->ICR=0xffffffff;

	// From WICED:
	// Must be lower priority than the value of configMAX_SYSCALL_INTERRUPT_PRIORITY
	// otherwise FreeRTOS will not be able to mask the interrupt
	// keep in mind that ARMCM3 interrupt priority logic is inverted, the highest value
	// is the lowest priority
	NVIC_EnableIRQ(SDIO_IRQn);
	NVIC_EnableIRQ(DMA2_Stream3_IRQn);

	return WWD_SUCCESS;
}

wwd_result_t host_platform_sdio_enumerate(void)
{
	uint32_t data=0;
	uint32_t tries=0;
	for(;;)
	{
		// Send CMD0 to set it to idle state.
		host_platform_sdio_transfer(BUS_WRITE,SDIO_CMD_0,SDIO_BYTE_MODE,SDIO_1B_BLOCK,0,0,0,NO_RESPONSE,NULL);

		// CMD5.
		host_platform_sdio_transfer(BUS_READ,SDIO_CMD_5,SDIO_BYTE_MODE,SDIO_1B_BLOCK,0,0,0,NO_RESPONSE,NULL);

		// Send CMD3 to get RCA.
		wwd_result_t result=host_platform_sdio_transfer(BUS_READ,SDIO_CMD_3,SDIO_BYTE_MODE,SDIO_1B_BLOCK,0,0,0,RESPONSE_NEEDED,&data);
		if(result==WWD_SUCCESS) break;

		tries++;
		if(tries>=MaxSDIOEnumerationRetries) return WWD_TIMEOUT;

		vTaskDelay(1);
	}

	// Send CMD7 with the returned RCA to select the card.
	host_platform_sdio_transfer(BUS_WRITE,SDIO_CMD_7,SDIO_BYTE_MODE,SDIO_1B_BLOCK,data, 0, 0,RESPONSE_NEEDED,NULL);

	return WWD_SUCCESS;
}

wwd_result_t host_platform_bus_deinit(void)
{
	vQueueDelete(SDIOTransferFinishedSemaphore);

	SDIO->MASK=0;
	SDIO->CLKCR=0;
	SDIO->POWER=0;

	SetAPB2PeripheralReset(RCC_APB2RSTR_SDIORST);
	ClearAPB2PeripheralReset(RCC_APB2RSTR_SDIORST);

	DisableAPB2PeripheralClock(RCC_APB2ENR_SDIOEN);

	NVIC_DisableIRQ(SDIO_IRQn);
	NVIC_DisableIRQ(DMA2_Stream3_IRQn);

	return WWD_SUCCESS;
}

static void SetupDMATransfer(wwd_bus_transfer_direction_t direction,
sdio_block_size_t blocksize,uint8_t *bytes,uint32_t length);

wwd_result_t host_platform_sdio_transfer(wwd_bus_transfer_direction_t direction,
sdio_command_t command,sdio_transfer_mode_t mode,sdio_block_size_t block_size,
uint32_t argument,uint32_t *data,uint16_t data_size,
sdio_response_needed_t response_expected,uint32_t *response)
{
	uint32_t loop_count = 0;
	uint32_t outertries=0;

	if(response) *response=0;

	// Stop DMA if it is active. TODO: Should it ever be?
	DMA2_Stream3->CR=0;

retry:
	SDIO->ICR=0xffffffff;

	outertries++;
	if(outertries>MaxTransferRetries)
	{
		SDIO->MASK=SDIO_MASK_SDIOITIE;
		return WWD_SDIO_RETRIES_EXCEEDED;
	}

	if(command==SDIO_CMD_53)
	{
		SDIOTransferFailed=false;

		// Enable interrupts.
		SDIO->MASK=SDIO_MASK_SDIOITIE|SDIO_MASK_CMDRENDIE|SDIO_MASK_CMDSENTIE;

		// Convert byte mode to blocks.
		if(mode==SDIO_BYTE_MODE)
		{
			if(data_size>256) block_size=512;
			else if(data_size>128) block_size=256;
			else if(data_size>64) block_size=128;
			else if(data_size>32) block_size=64;
			else if(data_size>16) block_size=32;
			else if(data_size>8) block_size=16;
			else if(data_size>4) block_size=8;
			else block_size=4;

			argument&=~0x1ff;
			argument|=block_size&0x1ff;
		}

		// Set up DMA controller for the transfer.
		if(direction==BUS_READ) SetupDMATransfer(direction,block_size,ReceiveBuffer,data_size);
		else SetupDMATransfer(direction,block_size,(uint8_t *)data,data_size);

		// Send command.
		SDIO->ARG=argument;
		SDIO->CMD=command|(SDIO_CMD_WAITRESP_0*1)|SDIO_CMD_CPSMEN;

		// Wait for the IRQ handler to signal the command has finished.
	    if(!xSemaphoreTake(SDIOTransferFinishedSemaphore,50*1000/configTICK_RATE_HZ))
		{
			SDIO->MASK=SDIO_MASK_SDIOITIE;
			// TODO: Should this return or retry? Shouldn't happen at all.
	        return WWD_TIMEOUT;
		}

		// If the IRQ handler detected errors, retry.
		if(SDIOTransferFailed) goto retry;

		// Check for further errors the IRQ handler does not deal with.
		// TODO: Why doesn't it?
		if(SDIO->STA&(SDIO_STA_DTIMEOUT|SDIO_STA_CTIMEOUT)) goto retry;

		// Wait for hardware to finish.
		// TODO: Doing what?
		uint32_t tries=0;
		for(;;)
		{
			uint32_t sta=SDIO->STA;

			// If there was an error, retry.
			if(sta&SDIOAllErrorsMask) goto retry;

			// Check for success and exit if so.
			if(!(sta&(SDIO_STA_TXACT|SDIO_STA_RXACT))) break;

			// Check for timeout and retry if needed.
			tries++;
			if(tries>=MaxDMACommandWaitLoops) goto retry;
		}

		if(direction==BUS_READ) memcpy(data,ReceiveBuffer,data_size);

		SDIO->MASK=SDIO_MASK_SDIOITIE;
	}
	else
	{
		SDIO->ARG=argument;
		SDIO->CMD=command|(SDIO_CMD_WAITRESP_0*1)|SDIO_CMD_CPSMEN;

		uint32_t tries=0;
		for(;;)
		{
			uint32_t sta=SDIO->STA;

			// If there was an error, retry.
			if(response_expected==RESPONSE_NEEDED && (sta&SDIOAllErrorsMask)) goto retry;

			// Check for success and exit if so.
			if(!(sta&SDIO_STA_CMDACT)) break;

			// Check for timeout and retry if needed.
			tries++;
			if(tries>=MaxNormalCommandWaitLoops) goto retry;
		}
	}

	if(response) *response=SDIO->RESP1;

	return WWD_SUCCESS;
}

static void SetupDMATransfer(wwd_bus_transfer_direction_t direction,
sdio_block_size_t blocksize,uint8_t *bytes,uint32_t length)
{
	uint32_t dmasize=(length+blocksize-1)/blocksize*blocksize;

	uint32_t dctrl=SDIO_DCTRL_DTEN|SDIO_DCTRL_DMAEN|SDIO_DCTRL_SDIOEN;

	if(direction==BUS_READ) dctrl|=SDIO_DCTRL_DTDIR;

	switch(blocksize)
	{
		case 1: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*0; break;
		case 2:	dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*1; break;
		case 4:	dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*2; break;
		case 8:	dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*3; break;
		case 16: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*4; break;
		case 32: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*5; break;
		case 64: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*6; break;
		case 128: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*7; break;
		case 256: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*8; break;
		case 512: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*9; break;
		case 1024: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*10; break;
		case 2048: dctrl|=SDIO_DCTRL_DBLOCKSIZE_0*11; break;
	}

	SDIO->DTIMER=0xffffffff;
	SDIO->DLEN=dmasize;
	SDIO->DCTRL=dctrl;

	if(direction==BUS_WRITE)
	{
		DMA2_Stream3->CR=(DMA_SxCR_DIR_0*1)| // Direction: Memory to peripheral
		(DMA_SxCR_CHSEL_0*4)| // Channel: 4
		DMA_SxCR_MINC| // Enable memory increment
		(DMA_SxCR_PSIZE_0*2)| // Peripheral size: word
		(DMA_SxCR_MSIZE_0*2)| // Memory size: word
		(DMA_SxCR_PL_0*3)| // Priority level: 3, very high
		(DMA_SxCR_PBURST_0*1)| // Peripheral burst: inc4
		(DMA_SxCR_MBURST_0*1)| // Memory burst: inc4
		DMA_SxCR_PFCTRL| // Peripheral flow control
		DMA_SxCR_TCIE; // Transfer complete interrupt enabled
	}
	else
	{
		DMA2_Stream3->CR=(DMA_SxCR_DIR_0*0)| // Direction: Peripheral to memory
		(DMA_SxCR_CHSEL_0*4)| // Channel: 4
		DMA_SxCR_MINC| // Enable memory increment
		(DMA_SxCR_PSIZE_0*2)| // Peripheral size: word
		(DMA_SxCR_MSIZE_0*2)| // Memory size: word
		(DMA_SxCR_PL_0*3)| // Priority level: 3, very high
		(DMA_SxCR_PBURST_0*1)| // Peripheral burst: inc4
		(DMA_SxCR_MBURST_0*1)| // Memory burst: inc4
		DMA_SxCR_PFCTRL| // Peripheral flow control
		DMA_SxCR_TCIE; // Transfer complete interrupt enabled
	}

	DMA2_Stream3->FCR=(DMA_SxFCR_FS_1*2)|(DMA_SxFCR_FTH_0*3)|DMA_SxFCR_DMDIS;
	DMA2_Stream3->PAR=(uint32_t)&SDIO->FIFO;
	DMA2_Stream3->M0AR=(uint32_t)bytes;
	DMA2_Stream3->NDTR=dmasize/4;
	DMA2->LIFCR=0x3f<<22;
}

void host_platform_enable_high_speed_sdio(void)
{
	SDIO->CLKCR=(SDIO_CLKCR_WIDBUS_0*1)|SDIO_CLKCR_CLKEN|0;
}

wwd_result_t host_platform_bus_enable_interrupt(void) { return WWD_SUCCESS; }

wwd_result_t host_platform_bus_disable_interrupt(void) { return WWD_SUCCESS; }

void host_platform_bus_buffer_freed(wwd_buffer_dir_t direction) {}




void SDIO_IRQHandler()
{
	signed portBASE_TYPE taskwoken=false;

	uint32_t intstatus=SDIO->STA;

	// Check for (some) errors.
	if(intstatus&(SDIO_STA_CCRCFAIL|SDIO_STA_DCRCFAIL|SDIO_STA_TXUNDERR|SDIO_STA_RXOVERR|SDIO_STA_STBITERR))
	{
		SDIOTransferFailed=true;
		SDIO->ICR=0xffffffff;
		xSemaphoreGiveFromISR(SDIOTransferFinishedSemaphore,&taskwoken);
	}

	// Check for command/response.
	if(intstatus&(SDIO_STA_CMDREND|SDIO_STA_CMDSENT))
	{
		SDIO->ICR=SDIO_STA_CMDREND|SDIO_STA_CMDSENT;

		if(SDIO->RESP1&0x800)
		{
			SDIOTransferFailed=true;
			xSemaphoreGiveFromISR(SDIOTransferFinishedSemaphore,&taskwoken);
		}
		else
		{
			// Start DMA transfer of block data.
			DMA2_Stream3->CR|=DMA_SxCR_EN;
		}
	}

	// Check for external interrupt.
	if(intstatus&SDIO_STA_SDIOIT)
	{
		SDIO->ICR=SDIO_ICR_SDIOITC;
		wwd_thread_notify_irq();
	}

	portEND_SWITCHING_ISR(taskwoken);
}

void DMA2_Stream3_IRQHandler()
{
	DMA2->LIFCR=0x3f<<22;

	signed portBASE_TYPE taskwoken=false;
	xSemaphoreGiveFromISR(SDIOTransferFinishedSemaphore,&taskwoken);
	portEND_SWITCHING_ISR(taskwoken);
}
