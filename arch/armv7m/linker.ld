/* RK0 Linker Script for QEMU Machine (lm3s6965evb) */

ENTRY(Reset_Handler)

/* Define memory layout (adjust sizes per device) */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

/* Stack pointer (MSP) is set to end of RAM */
_estack = ORIGIN(RAM) + LENGTH(RAM);
PROVIDE(__stack = _estack);  /* CMSIS/ARM-compatible symbol */

/* Sections */
SECTIONS
{
  .text :
  {
    KEEP(*(.isr_vector))     /* Vector table */
    *(.text*)                /* Application code */
    *(.rodata*)              /* Read-only data */
    KEEP(*(.init))
    KEEP(*(.fini))
  } > FLASH

  /* Initialised data: copied from flash to RAM */
  .data : AT(LOADADDR(.text) + SIZEOF(.text))
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM

  /* Uninitialised data */
  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM

  /* Provide symbol to copy .data from flash */
  _sidata = LOADADDR(.data);

  /* End of RAM marker */
  _end = .;
}
