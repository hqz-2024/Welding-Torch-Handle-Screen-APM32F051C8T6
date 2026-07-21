/*!
 * @file        lcd.h
 *
 * @brief       LCD drawing API header
 *
 * @version     V1.0.0
 *
 * @date        2026-07-15
 */

#ifndef __LCD_H
#define __LCD_H

#include "lcd_init.h"

/* ---------- Standard 16-bit colors (RGB565) ---------- */
#define WHITE          0xFFFF
#define BLACK          0x0000
#define BLUE           0x001F
#define BRED           0xF81F
#define GRED           0xFFE0
#define GBLUE          0x07FF
#define RED            0xF800
#define MAGENTA        0xF81F
#define GREEN          0x07E0
#define CYAN           0x7FFF
#define YELLOW         0xFFE0
#define BROWN          0xBC40
#define BRRED          0xFC07
#define GRAY           0x8430
#define DARKBLUE       0x01CF
#define LIGHTBLUE      0x7D7C
#define GRAYBLUE       0x5458
#define LIGHTGREEN     0x841F
#define LGRAY          0xC618
#define LGRAYBLUE      0xA651
#define LBBLUE         0x2B12

/* ---------- Drawing functions ---------- */
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);

/* ---------- Text display functions ---------- */
#define LCD_UI_FONT_ROW        0
#define LCD_UI_FONT_TITLE      1
#define LCD_UI_FONT_MAIN_NUM   2
#define LCD_UI_FONT_MAIN_TYPE  3
#define LCD_UI_FONT_TITLE_SMALL 4

uint8_t LCD_UI_TextWidth(const char *text, uint8_t fontId);
uint8_t LCD_UI_FontHeight(uint8_t fontId);
uint8_t LCD_UI_MaxCharWidth(const char *chars, uint8_t fontId);
void LCD_UI_ShowString(uint16_t x, uint16_t y, const char *text,
                       uint16_t fc, uint16_t bc, uint8_t fontId);
void LCD_UI_ShowFixedCellString(uint16_t x, uint16_t y, const char *text,
                                uint8_t cellW, uint8_t cellCount,
                                uint16_t fc, uint16_t bc, uint8_t fontId);
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);
void LCD_ShowScaledChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t scale);
void LCD_ShowScaledString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t scale);
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);
uint32_t mypow(uint8_t m, uint8_t n);

/* ---------- Picture display ---------- */
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t pic[]);

#endif /* __LCD_H */
