#include "System.h"

// PLL_VCO = (HSE_VALUE or HSI_VALUE / PLL_M) * PLL_N
// SYSCLK = PLL_VCO / PLL_P
// USB OTG FS, SDIO and RNG Clock =  PLL_VCO / PLLQ

#if defined(STM32F407xx)

#ifndef EnableOverclocking

// 8/4*336/4 = 168 MHz
#define PLL_M 4
#define PLL_N 336
#define PLL_P 4
#define PLL_Q 7

#else

// 8/4*352/4 = 176 MHz for more accurate VGA timings
#define PLL_M 4
#define PLL_N 352
#define PLL_P 4
#define PLL_Q 7

#endif

#elif defined(STM32F429xx)

// 8/4*360/4 = 180 MHz
#define PLL_M 4
#define PLL_N 360
#define PLL_P 4
#define PLL_Q 7

#elif defined(STM32F446xx)

// 8/4*360/4 = 180 MHz
#define PLL_M 4
#define PLL_N 360
#define PLL_P 4
#define PLL_Q 7

#else

#error This MCU is not supported.

#endif

// PLLI2S_VCO = (HSE_VALUE Or HSI_VALUE / PLL_M) * PLLI2S_N
// I2SCLK = PLLI2S_VCO / PLLI2S_R
#define PLLI2S_N 192
#define PLLI2S_R 5

static void InitializeClocks();

void InitializeSystem()
{
	#if (__FPU_PRESENT==1) && (__FPU_USED==1)
	SCB->CPACR|=((3<<10*2)|(3<<11*2));  // Set CP10 and CP11 Full Access 
	#endif

	// Reset the RCC clock configuration to the default reset state
	RCC->CR|=0x00000001; // Set HSION bit
	RCC->CFGR=0x00000000;	// Reset CFGR register
	RCC->CR&=0xFEF6FFFF; // Reset HSEON, CSSON and PLLON bits
	RCC->PLLCFGR=0x24003010; // Reset PLLCFGR register
	RCC->CR&=0xFFFBFFFF; // Reset HSEBYP bit
	RCC->CIR=0x00000000; // Disable all interrupts

	// Configure the System clock source, PLL Multiplier and Divider factors, 
	// AHB/APBx prescalers and Flash settings
	InitializeClocks();

	// Set vector table offset to flash memory start.
	SCB->VTOR=FLASH_BASE;

	// Set up interrupts to 4 bits preemption priority.
	SCB->AIRCR=0x05FA0000|0x300;
}

static void InitializeClocks()
{
	// PLL (clocked by HSE) used as System clock source.

	// Enable HSE and wait until it is read.
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
