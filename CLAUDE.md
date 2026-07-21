# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Geehy APM32F0xx SDK V1.8.6 — firmware library for APM32F0xx series ARM Cortex-M0+ MCUs. Uses a **Standard Peripheral Driver** (not HAL). Supports Keil MDK, IAR EWARM, and Eclipse CDT + GCC ARM Embedded toolchains.

## Build System

Three parallel project systems per example under `Examples/<peripheral>/<demo>/Project/`:

| IDE | Project File | Compiler |
|-----|-------------|----------|
| Keil MDK | `Project/MDK/*.uvprojx` | ARMCC |
| IAR EWARM | `Project/IAR/*.eww` + `*.ewp` | IAR |
| Eclipse CDT | `Project/Eclipse/` (.cproject + gcc linker scripts) | GCC ARM Embedded (`arm-none-eabi-gcc`) |

**No Makefile-based build.** Each example is a self-contained IDE project. The `Package/` directory holds Keil pack files (`.pack`) and an add-on installer for IDE integration.

### Linker Scripts

GCC linker scripts live in `Libraries/Device/Geehy/APM32F0xx/Source/gcc/` — named by chip variant and flash size (e.g., `gcc_APM32F05xx8.ld` → 64KB flash, 8KB RAM). Example projects also keep copies in their `Project/Eclipse/` folder.

## Architecture

```
CMSIS (core + system init)
  └── Device (startup files, vector table, linker scripts)
        └── StdPeriphDriver (apm32f0xx_gpio.c, apm32f0xx_spi.c, ...)
              └── Application (peripheral config via GPIO_Config_T structs, etc.)
```

### Directory Layout

- **`Boards/`** — Board BSP. `Board.h`/`Board.c` are switches; the actual implementation lives in per-board subdirectories (`Board_APM32F051_MINI/`, etc.). Select the target with a `BOARD_APM32F051_MINI` preprocessor define. BSP provides LED, button, and COM port init helpers.
- **`Libraries/CMSIS/`** — ARM CMSIS-Core (core_cm0.h, etc.) plus CMSIS-DSP and CMSIS-NN.
- **`Libraries/Device/Geehy/APM32F0xx/`** — MCU-specific: register defs (`apm32f0xx.h`, `apm32f051xx.h`), system init (`system_apm32f0xx.c`), startup files per toolchain (`.s` / `.S`). The umbrella header is `apm32f0xx.h`.
- **`Libraries/APM32F0xx_StdPeriphDriver/`** — Standard peripheral driver `.c`/`.h` pairs (gpio, spi, usart, adc, tmr, dma, i2c, etc.). API style: `GPIO_Config(GPIO_T, GPIO_Config_T*)`, `RCM_EnableAHBPeriphClock()`.
- **`Libraries/TSC_Device_Lib/`** — Touch sensing controller library (tsc, tsc_acq, tsc_dxs, tsc_filter, tsc_linrot, etc.).
- **`Middlewares/`** — FreeRTOS v202012.00, RTX5, RealThread RTOS, and APM32 USB Library (Device + Host stack).
- **`Examples/`** — One folder per peripheral (GPIO, SPI, I2C, ADC, USART, etc.), each containing demo project(s). The `Template/Template/` project is the starting point for new projects.
- **`Documents/`** — SDK user manual (`.chm`) and chip datasheet PDF.
- **`Package/`** — Keil DFP pack file (`Geehy.APM32F0xx_DFP.1.1.3.pack`), SVD files, and add-on installer.

### Standard Project Structure (per example)

```
Include/
  apm32f0xx_int.h    — interrupt handler declarations
  main.h             — app-level config
Source/
  main.c             — entry point (clock init done before main via SystemInit)
  apm32f0xx_int.c    — weak interrupt handlers (NMI, HardFault, SVCall, PendSV, SysTick)
  system_apm32f0xx.c — SystemInit(), SystemCoreClockUpdate()
Project/
  Eclipse/ IAR/ MDK/ — IDE project files + linker scripts
```

### Initialization Flow

`Reset_Handler` (startup) → `SystemInit()` (clock) → `main()` → configure peripherals → infinite loop / interrupt-driven.

### Driver API Patterns

All StdPeriphDriver modules follow a consistent pattern:
- Config struct passed to `_Config()` function (e.g., `GPIO_Config_T`, `SPI_Config_T`)
- Clock gating via `RCM_EnableAHBPeriphClock()` or `RCM_EnableAPB1/2PeriphClock()`
- Pin AF selection via `GPIO_Config_AF()` with `GPIO_AF_PINx` enum

## Project-Specific Context

The current hardware target is **APM32F051C8T6** (LQFP48, 64KB flash, 8KB RAM) driving:
- **Display**: ST7735S 8-pin SPI LCD via SPI2 (PB12=DC, PB13=SCK, PB14=RES, PB15=MOSI), backlight on PA8
- **Input**: 4 buttons (PB1, PB2, PB10, PB11 — active low, external pull-up)
- **RS485**: USART1 via MS2548 transceiver (PB6=TX, PB7=RX)
- **Debug**: SWD on PA13/PA14
- IO usage is fully documented in `apm32f051c8t6_io_usage.md` at the repo root.

This is a **welding torch handle screen** product.
