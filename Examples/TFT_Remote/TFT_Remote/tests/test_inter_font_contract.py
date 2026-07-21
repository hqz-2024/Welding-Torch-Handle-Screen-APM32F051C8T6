import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LCD_C = ROOT / "HARDWARE" / "LCD" / "lcd.c"
LCD_H = ROOT / "HARDWARE" / "LCD" / "lcd.h"
MAIN_C = ROOT / "Source" / "main.c"
FONT_H = ROOT / "HARDWARE" / "LCD" / "ui_font_inter.h"


class InterFontContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lcd_c = LCD_C.read_text(encoding="utf-8")
        cls.lcd_h = LCD_H.read_text(encoding="utf-8")
        cls.main_c = MAIN_C.read_text(encoding="utf-8")
        cls.font_h = FONT_H.read_text(encoding="utf-8") if FONT_H.exists() else ""

    def test_inter_font_header_is_generated_from_ttf(self):
        self.assertTrue(FONT_H.exists())
        for token in [
            "Generated from Inter-VariableFont_opsz,wght.ttf",
            "UI_FONT_ROW",
            "UI_FONT_TITLE",
            "UI_FONT_TITLE_SMALL",
            "UI_FONT_MAIN_NUM",
            "UI_FONT_MAIN_TYPE",
            "UiFont_T",
        ]:
            self.assertIn(token, self.font_h)
        self.assertIn("Inter Medium, 10px", self.font_h)
        self.assertIn("Inter SemiBold, 14px", self.font_h)
        self.assertIn("Inter SemiBold, 20px", self.font_h)

    def test_lcd_exposes_inter_text_api(self):
        for token in [
            "LCD_UI_TextWidth",
            "LCD_UI_FontHeight",
            "LCD_UI_MaxCharWidth",
            "LCD_UI_ShowString",
            "LCD_UI_ShowFixedCellString",
            "LCD_UI_FONT_ROW",
            "LCD_UI_FONT_TITLE",
            "LCD_UI_FONT_TITLE_SMALL",
            "LCD_UI_FONT_MAIN_NUM",
            "LCD_UI_FONT_MAIN_TYPE",
        ]:
            self.assertIn(token, self.lcd_h)
            self.assertIn(token, self.lcd_c)
        self.assertIn('#include "ui_font_inter.h"', self.lcd_c)

    def test_ui_uses_inter_for_all_visible_text(self):
        for token in [
            "LCD_UI_ShowString",
            "LCD_UI_TextWidth",
            "LCD_UI_FontHeight",
            "LCD_UI_FONT_ROW",
            "LCD_UI_FONT_TITLE",
            "LCD_UI_FONT_TITLE_SMALL",
            "LCD_UI_FONT_MAIN_NUM",
            "LCD_UI_FONT_MAIN_TYPE",
        ]:
            self.assertIn(token, self.main_c)

        self.assertNotIn("LCD_ShowScaledString", self.main_c)
        self.assertNotIn("ScaledTextWidth", self.main_c)


if __name__ == "__main__":
    unittest.main()
