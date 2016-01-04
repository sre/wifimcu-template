ENTRY(InterruptVectors)

MEMORY
{
	flash (rx)		: ORIGIN = 0x08000000, LENGTH = 512K
	sram (rwx)		: ORIGIN = 0x20000000, LENGTH = 128K-0x200
}

__isr_vector_sram = ORIGIN(sram) + LENGTH(sram);

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
	} > sram
	.bss :
	{
		_bss = .;
		*(.bss*)
		*(COMMON)
		_ebss = .;
	} > sram

	_heap = _ebss;
	_eheap = __isr_vector_sram;

	/DISCARD/ :
	{
		*(.ARM.exidx)
	}
}
