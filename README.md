# RK0 with QEMU 

This branch provides a building system example for _RK0_ running on QEMU.

## Requirements

- ARM GCC Toolchain (`arm-none-eabi-gcc`) (the files here do not contain any Newlib syscalls)
- QEMU for ARM (`qemu-system-arm`)
 
## Target Platform

This branch uses the `lm3s6965evb` QEMU machine. This emulates the Texas Instruments Stellaris LM3S6965, based on ARM Cortex-M3. 

## Project Structure

```
├── Src/                # RK0 kernel source files
├── Inc/                # RK0 kernel header files
├── app/                # Sample application
│   ├── Src/            # Application source files
│   └── Inc/            # Application header files
├── Build/              # Build output directory 
├── Lib/                # Output directory for the compiled library  
├── Linker/             # Linker scripts (these are created during build using the .ld in the root directory)
├── main.c              # Application entry point
├── startup_cortexm.c   # MCU startup code
├── cortex_m_generic.ld # Generic linker script
├── lm3s6965_flash_M3.ld # Target-specific linker script
└── Makefile            # Build system
```

## Building and Running

### Build for Cortex-M3 and Run in QEMU

Make sure the `Build`, `Lib` and `Linker` directories exist. 

```bash
make qemu-m3
```

> **Note**: The `lm3s6965evb` machine model in QEMU is specifically a Cortex-M3 implementation. The options for M4 and M7 _will not work without appropriate linker scripts and QEMU machine options_.

> For debugging use `make qemu-debug` to create a GDB target; on another terminal issue `make gdb` to connect to the local target.

## Customising

You can adjust the application by modifying the files in the `app/Src` directory. The main tasks are defined in `app/Src/application.c`. Also you need to configure the kernel in `Inc/kconfig.h` and `Inc/kenv.h`. 

This is a minimal sample with 3 tasks writing on the UART peripheral.
(It has been tested on QEMU-ARM 9.2.0/macOS 15.2)

![image](https://github.com/user-attachments/assets/9059c565-80ba-4829-9a45-683f4a7312c2)


