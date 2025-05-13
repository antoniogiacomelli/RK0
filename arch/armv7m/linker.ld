/* RK0 Linker Script for QEMU (lm3s6965evb) */

ENTRY(Reset_Handler)

/* Define memory layout */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

/* Stack pointer (MSP) is set to end of RAM */
__StackTop   = ORIGIN(RAM) + LENGTH(RAM);
_estack      = __StackTop;          /* CMSIS default name    */
PROVIDE(__stack = _estack);         /* compatibility symbol  */

/* Sections */
SECTIONS
{
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))     /* Vector table */
    *(.text*)                /* Application code */
    *(.rodata*)              /* Read-only data */
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    __etext = .;
  } > FLASH

  /* Initialised data: copied from flash to RAM */
 .data : AT(__etext)
  {
   . = ALIGN(4);
    _sdata = .;
    *(.data*)
    _edata = .;
    . = ALIGN(4);
  } > RAM
  
  /* Uninitialised data */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  . = ALIGN(4);
  } > RAM

  /* Provide symbol to copy .data from flash */
  _sidata = LOADADDR(.data);

    /* End of statically allocated data             */
  _end = .;                            
  PROVIDE(end = .);                   

}
