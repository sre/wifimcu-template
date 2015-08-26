ENTRY(InterruptVectors)

MEMORY
{
	flash (rx)		: ORIGIN = 0x08000000, LENGTH = 2048K
	ccmram (rwx)	: ORIGIN = 0x10000000, LENGTH = 64K
	sram (rwx)		: ORIGIN = 0x20000000, LENGTH = 192K
}

__isr_vector_sram = 0x20030000-0x200;

SECTIONS
{
    .text :
    {
        _text = .;
        __isr_vector_start = .;
        KEEP(*(.isr_vector))
        __isr_vector_end = .;
        *(.text*)
        *(.rodata*)
        _etext = .;
        _idata = .;
    } > flash
    .data : AT(ADDR(.text) + SIZEOF(.text))
    {
        _data = .;
        *(vtable)
        *(.data*)
        _edata = .;
    } > ccmram
    .bss :
    {
        _bss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > ccmram
}
