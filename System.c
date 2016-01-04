#include "System.h"
#include "RCC.h"
#include <errno.h>

// F_VCO = (HSE_VALUE or HSI_VALUE / PLL_M) * PLL_N
// 2 <= PLL_M <= 63
// 192 <= PLL_N <= 432
// 1 MHz <= (HSE_VALUE or HSI_VALUE / PLL_M) <= 2 MHz
// ??? 192 MHz <= (HSE_VALUE or HSI_VALUE / PLL_M) * PLL_N <= 432 MHz

// F_SYSCLK = F_VCO / PLL_P
// PLL_P = 2,4,6,8

// USB OTG FS, SDIO and RNG Clock = F_VCO / PLL_Q
// 2 <= PLL_Q <= 15
// USB OTG FS: PLL_VCO / PLLQ == 48 MHz
// SDIO and RNG Clock: PLL_VCO / PLLQ <= 48 MHz


#if defined(STM32F407xx)

#ifndef EnableOverclocking

// 8/4*336/4 = 168 MHz
// 8/4*336/14 = 48 MHz
#define PLL_M (HSEFrequency/2000000) // 8 MHz -> 4
#define PLL_N 336
#define PLL_P 4
#define PLL_Q 14

#else

// 8/4*352/4 = 176 MHz for more accurate VGA timings
// 8/4*352/15 = 46.9333 MHz
#define PLL_M (HSEFrequency/2000000) // 8 MHz -> 4
#define PLL_N 352
#define PLL_P 4
#define PLL_Q 15

#endif

#elif defined(STM32F411xE)

// 8/4*300/6 = 100 MHz
// 8/4*300/7 = 46.1538 MHz
#define PLL_M (HSEFrequency/2000000) // 8 MHz -> 4, 26 MHz -> 13 (WiFiMCU)
#define PLL_N 300
#define PLL_P 6
#define PLL_Q 13

#elif defined(STM32F429xx)

// 8/4*360/4 = 180 MHz
// 8/4*360/15 = 48 MHz
#define PLL_M (HSEFrequency/2000000) // 8 MHz -> 4
#define PLL_N 360
#define PLL_P 4
#define PLL_Q 15

#elif defined(STM32F446xx)

// 8/4*360/4 = 180 MHz
// 8/4*360/15 = 48 MHz
#define PLL_M (HSEFrequency/2000000) // 8 MHz -> 4
#define PLL_N 360
#define PLL_P 4
#define PLL_Q 15

#else

#error This MCU is not supported.

#endif

// PLLI2S_VCO = (HSE_VALUE Or HSI_VALUE / PLL_M) * PLLI2S_N
// I2SCLK = PLLI2S_VCO / PLLI2S_R
#define PLLI2S_N 192
#define PLLI2S_R 5

static void InitialiseClocks();

void InitialiseSystem()
{
	#if (__FPU_PRESENT==1) && (__FPU_USED==1)
	SCB->CPACR|=((3<<10*2)|(3<<11*2));  // Set CP10 and CP11 Full Access 
	#endif

	// Reset the RCC clock configuration to the default reset state
	RCC->CR|=RCC_CR_HSION; // Set HSION bit
	RCC->CFGR=0x00000000;	// Reset CFGR register
	RCC->CR&=~(RCC_CR_HSEON|RCC_CR_CSSON|RCC_CR_PLLON); // Reset HSEON, CSSON and PLLON bits
	RCC->PLLCFGR=0x20000000|(RCC_PLLCFGR_PLLQ_0*0x4)|(RCC_PLLCFGR_PLLN_0*0xc0)|(RCC_PLLCFGR_PLLM_0*0x10); // Reset PLLCFGR register
	RCC->CR&=~(RCC_CR_HSEBYP); // Reset HSEBYP bit
	RCC->CIR=0x00000000; // Disable all interrupts

	// Configure the System clock source, PLL Multiplier and Divider factors, 
	// AHB/APBx prescalers and Flash settings
	InitialiseClocks();

	// Set vector table offset to flash memory start.
	SCB->VTOR=FLASH_BASE;

	// Set up interrupts to 4 bits preemption priority.
	SCB->AIRCR=0x05FA0000|0x300;
}

static void InitialiseClocks()
{
	// PLL (clocked by HSE) used as System clock source.

	// Enable HSE and wait until it is ready.
	RCC->CR|=RCC_CR_HSEON;
	while(!(RCC->CR&RCC_CR_HSERDY));

	// Enable high performance mode, System frequency up to 168 MHz.
	RCC->APB1ENR|=RCC_APB1ENR_PWREN;
	PWR->CR|=PWR_CR_PMODE;  

	RCC->CFGR|=RCC_CFGR_HPRE_DIV1; // HCLK = SYSCLK / 1
	RCC->CFGR|=RCC_CFGR_PPRE2_DIV2; // PCLK2 = HCLK / 2
	RCC->CFGR|=RCC_CFGR_PPRE1_DIV4; // PCLK1 = HCLK / 4

	// Configure the main PLL.
	RCC->PLLCFGR=PLL_M|(PLL_N<<6)|(((PLL_P>>1)-1)<<16)|(RCC_PLLCFGR_PLLSRC_HSE)|(PLL_Q<<24);

	// Enable the main PLL and wait until it is ready.
	RCC->CR|=RCC_CR_PLLON;
	while(!(RCC->CR&RCC_CR_PLLRDY));
   
	// Configure Flash prefetch, Instruction cache, Data cache and wait state
	FLASH->ACR=FLASH_ACR_ICEN|FLASH_ACR_DCEN|FLASH_ACR_LATENCY_5WS;

	// Select the main PLL as system clock source.
	RCC->CFGR&=~RCC_CFGR_SW;
	RCC->CFGR|=RCC_CFGR_SW_PLL;

	// Wait until the main PLL is used as system clock source.
	while((RCC->CFGR&RCC_CFGR_SWS)!=RCC_CFGR_SWS_PLL);
}



// Interrupt handlers.

static InterruptHandler **WritableInterruptTable();
void Default_Handler();

extern InterruptHandler *__isr_vector_sram[];

void InstallInterruptHandler(IRQn_Type interrupt,InterruptHandler handler)
{
	// TODO: Disable interrupts.
	InterruptHandler **currenttable=WritableInterruptTable();
	currenttable[interrupt+16]=handler;
}

void RemoveInterruptHandler(IRQn_Type interrupt,InterruptHandler handler)
{
	// TODO: Disable interrupts.
	InterruptHandler **currenttable=WritableInterruptTable();
	currenttable[interrupt+16]=Default_Handler;
}

static InterruptHandler **WritableInterruptTable()
{
	InterruptHandler **currenttable=(InterruptHandler **)SCB->VTOR;
	if((uint32_t)currenttable==FLASH_BASE)
	{
		InterruptHandler **flashtable=(InterruptHandler **)FLASH_BASE;
		currenttable=__isr_vector_sram;
		for(int i=0;i<NumberOfInterruptTableEntries;i++) currenttable[i]=flashtable[i];

		SCB->VTOR=(uint32_t)currenttable;
	}

	return currenttable;
}




// Heap for newlib

void *_sbrk(int incr)
{
	extern uint8_t _heap[];
	extern uint8_t _eheap[];
	static uint8_t *heapend=_heap;

	if(heapend+incr>_eheap)
	{
		errno=ENOMEM;
		return (void *)-1;
	}

	uint8_t *newheapstart=heapend; 
	heapend+=incr;

	return newheapstart;
}
