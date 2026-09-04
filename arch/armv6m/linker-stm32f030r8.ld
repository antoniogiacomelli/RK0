/* RK0 Linker Script for STM32F030R8 / NUCLEO-F030R8 */

ENTRY(Reset_Handler)

_Min_Heap_Size  = 0x00000100;
_Min_Stack_Size = 0x00000400;

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 8K
}

__StackTop   = ORIGIN(RAM) + LENGTH(RAM);
_estack      = __StackTop;
PROVIDE(__stack = _estack);

SECTIONS
{
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
    KEEP(*(.ARM.exidx*))
    . = ALIGN(4);
    __etext = .;
  } > FLASH

  .data : AT(__etext)
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    *(.tableGetReady)
    _edata = .;
    . = ALIGN(4);
  } > RAM

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
    . = ALIGN(4);
  } > RAM

  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    *(.noinit)
    . = ALIGN(4);
  } > RAM

  _sidata = LOADADDR(.data);

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM

  /DISCARD/ :
  {
    libc.a   (*)
    libm.a   (*)
    libgcc.a (*)
  }
}
