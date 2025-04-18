/* Linker script for Cortex-M0 (micro:bit) */

ENTRY(Reset_Handler)

/* Define memory regions */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 16K
}

/* Define symbols for the stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Sections */
SECTIONS
{
  .text :
  {
    KEEP(*(.isr_vector))     /* Vector table */
    *(.text*)                /* Code */
    *(.rodata*)              /* Read-only data */
    KEEP(*(.init))
    KEEP(*(.fini))
  } > FLASH

  /* Initialized data section */
  .data : AT (ADDR(.text) + SIZEOF(.text))
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM

  /* Uninitialized data section */
  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM

  /* Provide symbols for copying .data section */
  _sidata = LOADADDR(.data);

  /* End of RAM section (optional marker) */
  _end = .;
}
