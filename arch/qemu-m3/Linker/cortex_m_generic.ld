/* 
 * Generic linker script for Cortex-M processors
 * for RK0 and QEMU lm3s6965evb target
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Specify the memory areas */
/* Memory regions are defined in the specific board script that includes this file */

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))         /* Startup code */
    . = ALIGN(4);
    *(.text)                     /* .text sections (code) */
    *(.text*)                    /* .text* sections (code) */
    *(.rodata)                   /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)                  /* .rodata* sections (constants, strings, etc.) */
    *(.glue_7)                   /* glue arm to thumb code */
    *(.glue_7t)                  /* glue thumb to arm code */
    . = ALIGN(4);
    _etext = .;                  /* define a global symbols at end of code */
  } >FLASH

  /* .ARM.extab section containing exception unwinding information */
  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  /* .ARM.exidx section containing index entries for section unwinding */
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } >FLASH

  /* Used by the startup to initialise data */
  _sidata = LOADADDR(.data);

  /* Initialised data sections into RAM */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;                  /* create a global symbol at data start */
    *(.data)                     /* .data sections */
    *(.data*)                    /* .data* sections */
    . = ALIGN(4);
    _edata = .;                  /* define a global symbol at data end */
  } >RAM AT> FLASH  

  /* Uninitialized data section into RAM */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;                   /* define a global symbol at bss start */
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;                   /* define a global symbol at bss end */
  } >RAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + 0x400;               /* Reserve 1K for stack */
    . = . + 0x400;               /* Reserve 1K for heap */
    . = ALIGN(8);
  } >RAM

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }
}
