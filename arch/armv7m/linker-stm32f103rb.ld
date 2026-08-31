/* RK0 Linker Script for STM32F103RB / NUCLEO-F103RB */

ENTRY(Reset_Handler)
_Min_Heap_Size  = 0x00000200; /* u can use the heap to place partition pools */
_Min_Stack_Size = 0x00000800; /* required amount of stack */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* Stack pointer (MSP) is set to end of RAM */
__StackTop   = ORIGIN(RAM) + LENGTH(RAM);
_estack      = __StackTop;          /* CMSIS default name    */
PROVIDE(__stack = _estack);         /* compatibility symbol  */

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
    KEEP(*(.ARM.exidx*))
    . = ALIGN(4);
    __etext = .;
  } > FLASH

 .data : AT(__etext)
  {
   . = ALIGN(4);
    _sdata = .;
    *(.data*)
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
  } >RAM

  _sidata = LOADADDR(.data);

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }
}
