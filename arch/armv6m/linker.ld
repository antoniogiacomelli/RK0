/* RK0 Linker Script for QEMU – BBC micro:bit (v1, nRF51822 - ARMv6-M)  */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}

__StackTop = ORIGIN(RAM) + LENGTH(RAM);
_estack    = __StackTop;      /* Traditional newlib / CMSIS name  */
PROVIDE(__stack = _estack);   /* Fallback for other toolchains     */

SECTIONS
{
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))      /* Interrupt / exception vectors      */
    KEEP(*(.init))
    KEEP(*(.fini))
    *(.text*)                 /* Application / library code         */
    *(.rodata*)               /* Read-only data, const strings …    */
    KEEP(*(.ARM.exidx*))      /* Exception unwind tables            */
    . = ALIGN(4);
    __etext = .;              /* End of code / ro-data in flash     */
  } > FLASH

  .data : AT(__etext)
  {
    . = ALIGN(4);
    KEEP(*(.tableGetReady))      /* Interrupt / exception vectors      */
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM

 


  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM



  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    *(.noinit*)
    . = ALIGN(4);
  } > RAM

  _end = .;             /* End of statically-allocated data  */
  PROVIDE(end = .);     /* Newlib / libc alias               */
}

_sidata = LOADADDR(.data);
