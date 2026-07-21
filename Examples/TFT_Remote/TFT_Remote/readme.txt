/*!
 * @file        readme.txt
 *
 * @brief       TFT_Remote - Welding Remote Control Project
 *
 * @version     V2.1.0
 *
 * @date        2026-07-15
 */

&par Project Description

Welding torch handle remote control demo.
Drives a 0.96-inch TFT IPS LCD (ST7735S, 80x160 portrait) via software SPI
with 4 navigation buttons (UP/DOWN/LEFT/RIGHT) on APM32F051C8T6.

Hardware connections:
  - TFT LCD:  PB13=SCK(SPI_CLK), PB15=MOSI(SPI_DATA),
              PB12=DC(Data/Cmd), PB14=RES(Reset), PA8=BLK(Backlight)
  - Buttons:  PB2=S2(UP), PB10=S3(DOWN), PB11=S4(LEFT), PB1=S1(RIGHT)
              (Active LOW, internal pull-up enabled)
  - SWD:      PA13=SWDIO, PA14=SWCLK

&par Button Operation

  UP (S2):     Previous parameter page
  DOWN (S3):   Next parameter page
  LEFT (S4):   Decrease selected parameter
  RIGHT (S1):  Increase selected parameter

  Parameters:
    CURRENT:  15A ~ 55A for PLATE/GOUGE, 15A ~ 30A for MESH/CLEAN
    TRIGGER:  2T / 4T
    POST GAS: 3s ~ 15s
    ARC TIME: 3s ~ 15s
    TYPE:     PLATE / MESH / CLEAN / GOUGE

  TYPE protocol values:
    PLATE: 0x0000
    MESH:  0x0001
    CLEAN: 0x0002
    GOUGE: 0x0003

&par Directory contents

  - HARDWARE/LCD/lcd_init.c/h    LCD GPIO init, software SPI, ST7735S register init
  - HARDWARE/LCD/lcd.c/h         LCD drawing API (fill, line, rect, circle, text, scaled text)
  - HARDWARE/LCD/lcdfont.h       ASCII font data (6x12, 8x16)
  - Source/main.c                Main application (button handling + UI)
  - Source/apm32f0xx_int.c       Interrupt handlers
  - Include/main.h               Button pin definitions
  - Include/apm32f0xx_int.h      Interrupt handler declarations
  - Project/MDK/TFT_Remote.uvprojx   Keil MDK-ARM project file

&par Software environment

  - Keil MDK-ARM V5.x with Geehy APM32F0xx DFP installed
  - Open Project/MDK/TFT_Remote.uvprojx
  - Select target "APM32F030" for the current APM32F051C8 board configuration
  - Compile and download via SWD (J-Link / CMSIS-DAP / ST-Link)
