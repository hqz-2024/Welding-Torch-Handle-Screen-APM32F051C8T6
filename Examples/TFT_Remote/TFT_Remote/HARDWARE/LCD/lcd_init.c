/*!
 * @file        lcd_init.c
 *
 * @brief       LCD init — Hardware SPI2 (24 MHz), APM32F051C8T6
 *              SPI2: PB13=SCK(AF0), PB15=MOSI(AF0)
 *              GPIO: PB12=DC, PB14=RES, PA8=BLK
 *
 * @version     V2.0.1
 * @date        2026-07-15
 */

#include "lcd_init.h"

void busy_wait(uint32_t n)
{
    volatile uint32_t i;
    for (i = 0; i < n; i++) { __NOP(); }
}

void Delay_Ms(uint32_t ms)
{
    while (ms--) { busy_wait(2400); }
}

/* ------------------------------------------------------------------ */
/*  GPIO + SPI2 initialization                                         */
/* ------------------------------------------------------------------ */
void LCD_GPIO_Init(void)
{
    GPIO_Config_T  gcfg;
    SPI_Config_T   scfg;

    RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_GPIOA | RCM_AHB_PERIPH_GPIOB);
    RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_SPI2);

    /* DC (PB12) & RES (PB14) — push-pull outputs */
    gcfg.pin     = LCD_DC_PIN | LCD_RES_PIN;
    gcfg.mode    = GPIO_MODE_OUT;
    gcfg.outtype = GPIO_OUT_TYPE_PP;
    gcfg.speed   = GPIO_SPEED_50MHz;
    gcfg.pupd    = GPIO_PUPD_NO;
    GPIO_Config(LCD_DC_PORT, &gcfg);

    /* BLK (PA8) */
    gcfg.pin     = LCD_BLK_PIN;
    gcfg.mode    = GPIO_MODE_OUT;
    GPIO_Config(LCD_BLK_PORT, &gcfg);

    /* SCK (PB13) & MOSI (PB15) — alternate function push-pull */
    gcfg.pin     = LCD_SCK_PIN | LCD_MOSI_PIN;
    gcfg.mode    = GPIO_MODE_AF;
    gcfg.outtype = GPIO_OUT_TYPE_PP;
    gcfg.speed   = GPIO_SPEED_50MHz;
    gcfg.pupd    = GPIO_PUPD_NO;
    GPIO_Config(LCD_SCLK_PORT, &gcfg);

    GPIO_ConfigPinAF(LCD_SCLK_PORT,  LCD_SCK_SRC,  GPIO_AF_PIN0);
    GPIO_ConfigPinAF(LCD_MOSI_PORT, LCD_MOSI_SRC, GPIO_AF_PIN0);

    SPI_Reset(LCD_SPI);

    /* SPI2: Master, 8-bit, CPOL=0 CPHA=0, MSB first, baud = PCLK/2 = 24 MHz */
    scfg.mode        = SPI_MODE_MASTER;
    scfg.length      = SPI_DATA_LENGTH_8B;
    scfg.phase       = SPI_CLKPHA_1EDGE;
    scfg.polarity    = SPI_CLKPOL_LOW;
    scfg.slaveSelect = SPI_SSC_ENABLE;   /* software NSS — avoid PB12/DC conflict */
    scfg.firstBit    = SPI_FIRST_BIT_MSB;
    scfg.direction   = SPI_DIRECTION_2LINES_FULLDUPLEX;
    scfg.baudrateDiv = SPI_BAUDRATE_DIV_2;
    scfg.crcPolynomial = 7;
    SPI_Config(LCD_SPI, &scfg);

    SPI_EnableInternalSlave(LCD_SPI);
    SPI_Enable(LCD_SPI);

    LCD_DC_H();
    LCD_RES_H();
    LCD_BLK_H();
}

/* ------------------------------------------------------------------ */
/*  SPI send using SDK polling API                                     */
/* ------------------------------------------------------------------ */
void LCD_SPI_WaitDone(void)
{
    while (!SPI_ReadStatusFlag(LCD_SPI, SPI_FLAG_TXBE));
    while (SPI_ReadStatusFlag(LCD_SPI, SPI_FLAG_BUSY));
}

void LCD_SPI_SendByte(uint8_t dat)
{
    while (!SPI_ReadStatusFlag(LCD_SPI, SPI_FLAG_TXBE));
    SPI_TxData8(LCD_SPI, dat);
}

void LCD_SPI_SendColorRepeat(uint16_t color, uint32_t count)
{
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)color;

    while (count--) {
        LCD_SPI_SendByte(hi);
        LCD_SPI_SendByte(lo);
    }

    LCD_SPI_WaitDone();
}

void LCD_WR_DATA8(uint8_t dat) { LCD_SPI_SendByte(dat); }

void LCD_WR_DATA(uint16_t dat)
{
    LCD_SPI_SendByte((uint8_t)(dat >> 8));
    LCD_SPI_SendByte((uint8_t)(dat));
}

void LCD_WR_REG(uint8_t dat)
{
    LCD_SPI_WaitDone();
    LCD_DC_L();
    LCD_SPI_SendByte(dat);
    LCD_SPI_WaitDone();
    LCD_DC_H();
}

/* ------------------------------------------------------------------ */
/*  Set drawing window                                                 */
/* ------------------------------------------------------------------ */
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_WR_REG(0x2A); LCD_WR_DATA(x1 + 24); LCD_WR_DATA(x2 + 24);
    LCD_WR_REG(0x2B); LCD_WR_DATA(y1);      LCD_WR_DATA(y2);
    LCD_WR_REG(0x2C);
}

/* ------------------------------------------------------------------ */
/*  ST7735S register init                                              */
/* ------------------------------------------------------------------ */
void LCD_Init(void)
{
    LCD_GPIO_Init();

    /* Hardware reset */
    LCD_RES_L(); Delay_Ms(20);
    LCD_RES_H(); Delay_Ms(20);

    LCD_BLK_H();

    LCD_WR_REG(0x11); Delay_Ms(50);     /* Sleep out */

    LCD_WR_REG(0xB1); LCD_WR_DATA8(0x05); LCD_WR_DATA8(0x3C); LCD_WR_DATA8(0x3C);
    LCD_WR_REG(0xB2); LCD_WR_DATA8(0x05); LCD_WR_DATA8(0x3C); LCD_WR_DATA8(0x3C);
    LCD_WR_REG(0xB3); LCD_WR_DATA8(0x05); LCD_WR_DATA8(0x3C); LCD_WR_DATA8(0x3C);
                       LCD_WR_DATA8(0x05); LCD_WR_DATA8(0x3C); LCD_WR_DATA8(0x3C);
    LCD_WR_REG(0xB4); LCD_WR_DATA8(0x03);
    LCD_WR_REG(0xC0); LCD_WR_DATA8(0x0E); LCD_WR_DATA8(0x0E); LCD_WR_DATA8(0x04);
    LCD_WR_REG(0xC1); LCD_WR_DATA8(0xC5);
    LCD_WR_REG(0xC2); LCD_WR_DATA8(0x0D); LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xC3); LCD_WR_DATA8(0x8D); LCD_WR_DATA8(0x2A);
    LCD_WR_REG(0xC4); LCD_WR_DATA8(0x8D); LCD_WR_DATA8(0xEE);
    LCD_WR_REG(0xC5); LCD_WR_DATA8(0x06);

    LCD_WR_REG(0x36); LCD_WR_DATA8(0x08);  /* Portrait */
    LCD_WR_REG(0x3A); LCD_WR_DATA8(0x55);  /* 16-bit color */

    LCD_WR_REG(0xE0);
    LCD_WR_DATA8(0x0B); LCD_WR_DATA8(0x17); LCD_WR_DATA8(0x0A); LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x1A); LCD_WR_DATA8(0x19); LCD_WR_DATA8(0x16); LCD_WR_DATA8(0x1D);
    LCD_WR_DATA8(0x21); LCD_WR_DATA8(0x26); LCD_WR_DATA8(0x37); LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x00); LCD_WR_DATA8(0x09); LCD_WR_DATA8(0x05); LCD_WR_DATA8(0x10);

    LCD_WR_REG(0xE1);
    LCD_WR_DATA8(0x0C); LCD_WR_DATA8(0x19); LCD_WR_DATA8(0x09); LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x1B); LCD_WR_DATA8(0x19); LCD_WR_DATA8(0x15); LCD_WR_DATA8(0x1D);
    LCD_WR_DATA8(0x21); LCD_WR_DATA8(0x26); LCD_WR_DATA8(0x39); LCD_WR_DATA8(0x3E);
    LCD_WR_DATA8(0x00); LCD_WR_DATA8(0x09); LCD_WR_DATA8(0x05); LCD_WR_DATA8(0x10);

    Delay_Ms(10);
    LCD_WR_REG(0x29);  /* Display on */
}
