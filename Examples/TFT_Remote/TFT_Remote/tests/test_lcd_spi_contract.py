import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LCD_INIT_C = ROOT / "HARDWARE" / "LCD" / "lcd_init.c"
LCD_INIT_H = ROOT / "HARDWARE" / "LCD" / "lcd_init.h"
LCD_C = ROOT / "HARDWARE" / "LCD" / "lcd.c"


class LcdSpiContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.init_c = LCD_INIT_C.read_text(encoding="utf-8")
        cls.init_h = LCD_INIT_H.read_text(encoding="utf-8")
        cls.lcd_c = LCD_C.read_text(encoding="utf-8")

    def test_spi2_hardware_mode_uses_24mhz_software_nss(self):
        for token in [
            "RCM_APB1_PERIPH_SPI2",
            "GPIO_MODE_AF",
            "GPIO_AF_PIN0",
            "SPI_MODE_MASTER",
            "SPI_BAUDRATE_DIV_2",
            "SPI_SSC_ENABLE",
            "SPI_EnableInternalSlave",
        ]:
            self.assertIn(token, self.init_c)
        self.assertNotIn("SPI_SSC_DISABLE", self.init_c)

    def test_byte_send_does_not_wait_busy_per_byte(self):
        send_byte = re.search(
            r"void LCD_SPI_SendByte\(uint8_t dat\)\s*\{(?P<body>.*?)\n\}",
            self.init_c,
            re.S,
        )
        self.assertIsNotNone(send_byte)
        body = send_byte.group("body")
        self.assertIn("SPI_FLAG_TXBE", body)
        self.assertIn("SPI_TxData8", body)
        self.assertNotIn("SPI_FLAG_BUSY", body)

    def test_register_write_flushes_before_dc_edges(self):
        self.assertIn("void LCD_SPI_WaitDone(void)", self.init_c)
        wr_reg = re.search(
            r"void LCD_WR_REG\(uint8_t dat\)\s*\{(?P<body>.*?)\n\}",
            self.init_c,
            re.S,
        )
        self.assertIsNotNone(wr_reg)
        body = wr_reg.group("body")
        self.assertRegex(body, r"LCD_SPI_WaitDone\(\);\s*LCD_DC_L")
        self.assertRegex(body, r"LCD_SPI_SendByte\(dat\);\s*LCD_SPI_WaitDone\(\);\s*LCD_DC_H")

    def test_bulk_fill_uses_spi_repeat_burst(self):
        self.assertIn("void LCD_SPI_SendColorRepeat(uint16_t color, uint32_t count)", self.init_h)
        self.assertIn("LCD_SPI_SendColorRepeat", self.init_c)
        fill = re.search(
            r"void LCD_Fill\(.*?\)\s*\{(?P<body>.*?)\n\}",
            self.lcd_c,
            re.S,
        )
        self.assertIsNotNone(fill)
        body = fill.group("body")
        self.assertIn("LCD_SPI_SendColorRepeat", body)
        self.assertNotIn("LCD_WR_DATA(color)", body)


if __name__ == "__main__":
    unittest.main()
