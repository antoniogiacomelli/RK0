/*
 * Linker script for TI LM3S6965 Cortex-M3 with QEMU lm3s6965evb
 * For use with RK0 RTOS
 */

/* Define the memory regions to match the lm3s6965 */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x00000000, LENGTH = 256K   /* Flash memory */
  RAM (rwx)       : ORIGIN = 0x20000000, LENGTH = 64K    /* RAM area */
}

/* Include the common parts of the linker script */
INCLUDE cortex_m_generic.ld