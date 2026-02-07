/* RK0 Linker Script for QEMU (ARMv6-M / micro:bit, nRF51822) */

ENTRY(Reset_Handler)

/* You can use the heap to place partition pools */
_Min_Heap_Size  = 0x00000200;
_Min_Stack_Size = 0x00000400;

/* Define memory layout */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 16K
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
    KEEP(*(.ARM.exidx*))
    . = ALIGN(4);
    __etext = .;
  } > FLASH

  /* Initialised data: copied from flash to RAM */
  .data : AT(__etext)
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    *(.tableGetReady)
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

  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    *(.noinit)
    . = ALIGN(4);
  } > RAM

  /* Provide symbol to copy .data from flash */
  _sidata = LOADADDR(.data);

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end   = .);
    PROVIDE(_end  = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM

  /* this is actually doing nothing, as we link libcnano; just in case */
  /DISCARD/ :
  {
    libc.a   (*)
    libm.a   (*)
    libgcc.a (*)
  }
}
