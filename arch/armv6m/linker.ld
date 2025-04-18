/* Linker script for BBC micro:bit (nRF51822) */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 16K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data : 
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
    
    /* Stack and heap definitions */
    .stack :
    {
        _stack_size = 2048;
        . = ALIGN(8);
        . = . + _stack_size;
        . = ALIGN(8);
        _stack_top = .;
    } > RAM

    /* Remove information from the standard libraries */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }
}