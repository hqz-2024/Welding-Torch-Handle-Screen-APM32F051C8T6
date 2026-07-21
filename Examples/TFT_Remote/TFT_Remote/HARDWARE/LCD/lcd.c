/*!
 * @file        lcd.c
 *
 * @brief       LCD drawing functions (fill, point, line, rect, circle, text)
 *
 * @version     V1.0.0
 *
 * @date        2026-07-15
 */

#include "lcd.h"
#include "lcdfont.h"
#include "ui_font_inter.h"

/* ------------------------------------------------------------------ */
/*  Fill a rectangular area with a solid color                         */
/* ------------------------------------------------------------------ */
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color)
{
    uint32_t count;

    LCD_Address_Set(xsta, ysta, xend - 1, yend - 1);
    count = (uint32_t)(xend - xsta) * (uint32_t)(yend - ysta);
    LCD_SPI_SendColorRepeat(color, count);
}

/* ------------------------------------------------------------------ */
/*  Draw a single pixel                                                */
/* ------------------------------------------------------------------ */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_Address_Set(x, y, x, y);
    LCD_WR_DATA(color);
}

/* ------------------------------------------------------------------ */
/*  Draw a line (Bresenham algorithm)                                  */
/* ------------------------------------------------------------------ */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int32_t  xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int32_t  incx, incy, uRow, uCol;

    delta_x = (int32_t)x2 - (int32_t)x1;
    delta_y = (int32_t)y2 - (int32_t)y1;
    uRow = x1;
    uCol = y1;

    if (delta_x > 0) {
        incx = 1;
    } else if (delta_x == 0) {
        incx = 0;
    } else {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0) {
        incy = 1;
    } else if (delta_y == 0) {
        incy = 0;
    } else {
        incy = -1;
        delta_y = -delta_y;
    }

    distance = (delta_x > delta_y) ? delta_x : delta_y;

    for (t = 0; t <= (uint16_t)distance; t++) {
        LCD_DrawPoint((uint16_t)uRow, (uint16_t)uCol, color);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance) {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Draw rectangle outline                                             */
/* ------------------------------------------------------------------ */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_DrawLine(x1, y1, x2, y1, color);
    LCD_DrawLine(x1, y1, x1, y2, color);
    LCD_DrawLine(x1, y2, x2, y2, color);
    LCD_DrawLine(x2, y1, x2, y2, color);
}

/* ------------------------------------------------------------------ */
/*  Draw a filled circle (Bresenham)                                   */
/* ------------------------------------------------------------------ */
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int32_t a, b;
    a = 0;
    b = r;

    while (a <= b) {
        LCD_DrawPoint(x0 - b, y0 - a, color);
        LCD_DrawPoint(x0 + b, y0 - a, color);
        LCD_DrawPoint(x0 - a, y0 + b, color);
        LCD_DrawPoint(x0 - a, y0 - b, color);
        LCD_DrawPoint(x0 + b, y0 + a, color);
        LCD_DrawPoint(x0 + a, y0 - b, color);
        LCD_DrawPoint(x0 + a, y0 + b, color);
        LCD_DrawPoint(x0 - b, y0 + a, color);
        a++;
        if ((a * a + b * b) > (r * r)) {
            b--;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Display a single ASCII character                                   */
/* ------------------------------------------------------------------ */
static const UiFont_T *LCD_UI_GetFont(uint8_t fontId)
{
    switch (fontId) {
    case LCD_UI_FONT_TITLE_SMALL:
        return &UI_FONT_TITLE_SMALL;
    case LCD_UI_FONT_TITLE:
        return &UI_FONT_TITLE;
    case LCD_UI_FONT_MAIN_NUM:
        return &UI_FONT_MAIN_NUM;
    case LCD_UI_FONT_MAIN_TYPE:
        return &UI_FONT_MAIN_TYPE;
    case LCD_UI_FONT_ROW:
    default:
        return &UI_FONT_ROW;
    }
}

static const UiGlyph_T *LCD_UI_FindGlyph(const UiFont_T *font, char code)
{
    uint8_t i;
    const UiGlyph_T *space = 0;

    for (i = 0; i < font->count; i++) {
        if (font->glyphs[i].code == code) {
            return &font->glyphs[i];
        }
        if (font->glyphs[i].code == ' ') {
            space = &font->glyphs[i];
        }
    }
    return space;
}

static uint16_t LCD_UI_Blend565(uint16_t fg, uint16_t bg, uint8_t alpha)
{
    uint16_t inv = (uint16_t)(255 - alpha);
    uint16_t fr = (uint16_t)((fg >> 11) & 0x1F);
    uint16_t fg6 = (uint16_t)((fg >> 5) & 0x3F);
    uint16_t fb = (uint16_t)(fg & 0x1F);
    uint16_t br = (uint16_t)((bg >> 11) & 0x1F);
    uint16_t bg6 = (uint16_t)((bg >> 5) & 0x3F);
    uint16_t bb = (uint16_t)(bg & 0x1F);
    uint16_t r = (uint16_t)((fr * alpha + br * inv + 127) / 255);
    uint16_t g = (uint16_t)((fg6 * alpha + bg6 * inv + 127) / 255);
    uint16_t b = (uint16_t)((fb * alpha + bb * inv + 127) / 255);

    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint8_t LCD_UI_TextWidth(const char *text, uint8_t fontId)
{
    const UiFont_T *font = LCD_UI_GetFont(fontId);
    const UiGlyph_T *glyph;
    uint16_t width = 0;

    while (*text != '\0') {
        glyph = LCD_UI_FindGlyph(font, *text);
        if (glyph != 0) {
            width += glyph->width;
        }
        text++;
    }

    return (width > 255) ? 255 : (uint8_t)width;
}

uint8_t LCD_UI_FontHeight(uint8_t fontId)
{
    return LCD_UI_GetFont(fontId)->height;
}

uint8_t LCD_UI_MaxCharWidth(const char *chars, uint8_t fontId)
{
    const UiFont_T *font = LCD_UI_GetFont(fontId);
    const UiGlyph_T *glyph;
    uint8_t maxWidth = 0;

    while (*chars != '\0') {
        glyph = LCD_UI_FindGlyph(font, *chars);
        if (glyph != 0 && glyph->width > maxWidth) {
            maxWidth = glyph->width;
        }
        chars++;
    }

    return maxWidth;
}

static void LCD_UI_ShowGlyph(uint16_t x, uint16_t y, const UiFont_T *font,
                             const UiGlyph_T *glyph, uint16_t fc, uint16_t bc)
{
    uint8_t row;
    uint8_t col;
    uint8_t width;
    uint8_t height;
    const uint8_t *bitmap;

    if (glyph == 0 || x >= LCD_W || y >= LCD_H) {
        return;
    }

    width = glyph->width;
    height = font->height;
    if ((uint16_t)(x + width) > LCD_W) {
        width = (uint8_t)(LCD_W - x);
    }
    if ((uint16_t)(y + height) > LCD_H) {
        height = (uint8_t)(LCD_H - y);
    }

    LCD_Address_Set(x, y, (uint16_t)(x + width - 1), (uint16_t)(y + height - 1));
    bitmap = glyph->bitmap;
    for (row = 0; row < height; row++) {
        for (col = 0; col < width; col++) {
            LCD_WR_DATA(LCD_UI_Blend565(fc, bc, bitmap[row * glyph->width + col]));
        }
    }
}

static void LCD_UI_ShowFixedCell(uint16_t x, uint16_t y, const UiFont_T *font,
                                 const UiGlyph_T *glyph, uint8_t cellW,
                                 uint16_t fc, uint16_t bc)
{
    uint8_t row;
    uint8_t col;
    uint8_t width;
    uint8_t height;
    uint8_t glyphX;
    uint8_t glyphCol;
    uint16_t color;
    const uint8_t *bitmap;

    if (cellW == 0 || x >= LCD_W || y >= LCD_H) {
        return;
    }

    width = cellW;
    height = font->height;
    if ((uint16_t)(x + width) > LCD_W) {
        width = (uint8_t)(LCD_W - x);
    }
    if ((uint16_t)(y + height) > LCD_H) {
        height = (uint8_t)(LCD_H - y);
    }

    glyphX = 0;
    if (glyph != 0 && cellW > glyph->width) {
        glyphX = (uint8_t)((cellW - glyph->width) / 2);
    }

    LCD_Address_Set(x, y, (uint16_t)(x + width - 1), (uint16_t)(y + height - 1));
    bitmap = (glyph == 0) ? 0 : glyph->bitmap;
    for (row = 0; row < height; row++) {
        for (col = 0; col < width; col++) {
            color = bc;
            if (glyph != 0 && col >= glyphX) {
                glyphCol = (uint8_t)(col - glyphX);
                if (glyphCol < glyph->width) {
                    color = LCD_UI_Blend565(fc, bc, bitmap[row * glyph->width + glyphCol]);
                }
            }
            LCD_WR_DATA(color);
        }
    }
}

void LCD_UI_ShowString(uint16_t x, uint16_t y, const char *text,
                       uint16_t fc, uint16_t bc, uint8_t fontId)
{
    const UiFont_T *font = LCD_UI_GetFont(fontId);
    const UiGlyph_T *glyph;

    while (*text != '\0' && x < LCD_W) {
        glyph = LCD_UI_FindGlyph(font, *text);
        if (glyph != 0) {
            LCD_UI_ShowGlyph(x, y, font, glyph, fc, bc);
            x = (uint16_t)(x + glyph->width);
        }
        text++;
    }
}

void LCD_UI_ShowFixedCellString(uint16_t x, uint16_t y, const char *text,
                                uint8_t cellW, uint8_t cellCount,
                                uint16_t fc, uint16_t bc, uint8_t fontId)
{
    const UiFont_T *font = LCD_UI_GetFont(fontId);
    const UiGlyph_T *glyph;
    uint8_t i;
    char ch;

    if (text == 0 || cellW == 0 || cellCount == 0) {
        return;
    }

    for (i = 0; i < cellCount && x < LCD_W; i++) {
        ch = (*text == '\0') ? ' ' : *text++;
        glyph = LCD_UI_FindGlyph(font, ch);
        LCD_UI_ShowFixedCell(x, y, font, glyph, cellW, fc, bc);
        x = (uint16_t)(x + cellW);
    }
}

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc,
                  uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t  temp, sizex, t, m = 0;
    uint16_t i, TypefaceNum;
    uint16_t x0 = x;

    sizex = sizey / 2;
    TypefaceNum = ((sizex / 8) + ((sizex % 8) ? 1 : 0)) * sizey;
    num = num - ' ';  /* offset from space character */

    LCD_Address_Set(x, y, x + sizex - 1, y + sizey - 1);

    for (i = 0; i < TypefaceNum; i++) {
        if (sizey == 12) {
            temp = ascii_1206[num][i];
        } else if (sizey == 16) {
            temp = ascii_1608[num][i];
        } else {
            return;
        }

        for (t = 0; t < 8; t++) {
            if (!mode) {
                if (temp & (0x01 << t)) {
                    LCD_WR_DATA(fc);
                } else {
                    LCD_WR_DATA(bc);
                }
                m++;
                if (m % sizex == 0) {
                    m = 0;
                    break;
                }
            } else {
                if (temp & (0x01 << t)) {
                    LCD_DrawPoint(x, y, fc);
                }
                x++;
                if ((x - x0) == sizex) {
                    x = x0;
                    y++;
                    break;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Display a null-terminated string                                   */
/* ------------------------------------------------------------------ */
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc,
                    uint16_t bc, uint8_t sizey, uint8_t mode)
{
    while (*p != '\0') {
        LCD_ShowChar(x, y, *p, fc, bc, sizey, mode);
        x += sizey / 2;
        p++;
    }
}

/* ------------------------------------------------------------------ */
/*  Display one ASCII character using the 8x16 font scaled by N        */
/* ------------------------------------------------------------------ */
void LCD_ShowScaledChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc,
                        uint16_t bc, uint8_t scale)
{
    uint8_t  row, bit;
    uint8_t  data;
    uint16_t px;
    uint16_t py;
    uint16_t color;

    if (scale == 0) {
        return;
    }

    num = num - ' ';
    for (row = 0; row < 16; row++) {
        data = ascii_1608[num][row];
        for (bit = 0; bit < 8; bit++) {
            color = (data & (0x01 << bit)) ? fc : bc;
            for (py = 0; py < scale; py++) {
                for (px = 0; px < scale; px++) {
                    LCD_DrawPoint(x + (uint16_t)bit * scale + px,
                                  y + (uint16_t)row * scale + py,
                                  color);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Display a scaled ASCII string                                      */
/* ------------------------------------------------------------------ */
void LCD_ShowScaledString(uint16_t x, uint16_t y, const uint8_t *p,
                          uint16_t fc, uint16_t bc, uint8_t scale)
{
    while (*p != '\0') {
        LCD_ShowScaledChar(x, y, *p, fc, bc, scale);
        x += (uint16_t)8 * scale;
        p++;
    }
}

/* ------------------------------------------------------------------ */
/*  Integer power utility                                              */
/* ------------------------------------------------------------------ */
uint32_t mypow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) {
        result *= m;
    }
    return result;
}

/* ------------------------------------------------------------------ */
/*  Display an unsigned integer with leading-space padding             */
/* ------------------------------------------------------------------ */
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len,
                    uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t t, temp;
    uint8_t enshow = 0;
    uint8_t sizex = sizey / 2;

    for (t = 0; t < len; t++) {
        temp = (uint8_t)((num / mypow(10, len - t - 1)) % 10);
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                LCD_ShowChar(x + t * sizex, y, ' ', fc, bc, sizey, 0);
                continue;
            } else {
                enshow = 1;
            }
        }
        LCD_ShowChar(x + t * sizex, y, temp + 48, fc, bc, sizey, 0);
    }
}

/* ------------------------------------------------------------------ */
/*  Display a raw picture (RGB565 byte array)                          */
/* ------------------------------------------------------------------ */
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t length, uint16_t width,
                     const uint8_t pic[])
{
    uint16_t i, j;
    uint32_t k = 0;

    LCD_Address_Set(x, y, x + length - 1, y + width - 1);

    for (i = 0; i < length; i++) {
        for (j = 0; j < width; j++) {
            LCD_WR_DATA8(pic[k * 2]);
            LCD_WR_DATA8(pic[k * 2 + 1]);
            k++;
        }
    }
}
