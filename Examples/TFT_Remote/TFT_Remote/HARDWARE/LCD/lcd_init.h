/*!
 * @file        lcd_init.h
 *
 * @brief       LCD initialization header — hardware SPI2
 *
 * @version     V2.0.0
 * @date        2026-07-15
 */

#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#include "apm32f0xx.h"
#include "apm32f0xx_gpio.h"
#include "apm32f0xx_rcm.h"
#include "apm32f0xx_spi.h"

/* Portrait 80 x 160 */
#define USE_HORIZONTAL 0

#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 1
#define LCD_W  80
#define LCD_H  160
#else
#define LCD_W  160
#define LCD_H  80
#endif

/* ---- Hardware SPI2 pins ---- */
#define LCD_SPI           SPI2
#define LCD_SCLK_PORT     GPIOB
#define LCD_SCK_PIN       GPIO_PIN_13
#define LCD_SCK_SRC       GPIO_PIN_SOURCE_13
#define LCD_MOSI_PORT     GPIOB
#define LCD_MOSI_PIN      GPIO_PIN_15
#define LCD_MOSI_SRC      GPIO_PIN_SOURCE_15

/* ---- GPIO pins (DC, RES, BLK) ---- */
#define LCD_DC_PORT       GPIOB
#define LCD_DC_PIN        GPIO_PIN_12
#define LCD_RES_PORT      GPIOB
#define LCD_RES_PIN       GPIO_PIN_14
#define LCD_BLK_PORT      GPIOA
#define LCD_BLK_PIN       GPIO_PIN_8

/* ---- Pin control macros (DC / RES / BLK only; SCK/MOSI handled by HW) ---- */
#define LCD_DC_H()   GPIO_SetBit(LCD_DC_PORT, LCD_DC_PIN)
#define LCD_DC_L()   GPIO_ClearBit(LCD_DC_PORT, LCD_DC_PIN)
#define LCD_RES_H()  GPIO_SetBit(LCD_RES_PORT, LCD_RES_PIN)
#define LCD_RES_L()  GPIO_ClearBit(LCD_RES_PORT, LCD_RES_PIN)
#define LCD_BLK_H()  GPIO_SetBit(LCD_BLK_PORT, LCD_BLK_PIN)
#define LCD_BLK_L()  GPIO_ClearBit(LCD_BLK_PORT, LCD_BLK_PIN)

void LCD_GPIO_Init(void);
void LCD_SPI_WaitDone(void);
void LCD_SPI_SendByte(uint8_t dat);
void LCD_SPI_SendColorRepeat(uint16_t color, uint32_t count);
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void LCD_WR_REG(uint8_t dat);
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Init(void);
void Delay_Ms(uint32_t ms);
void busy_wait(uint32_t n);

#endif
