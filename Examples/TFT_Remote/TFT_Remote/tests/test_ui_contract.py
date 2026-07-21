import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN_C = ROOT / "Source" / "main.c"


class TftRemoteUiContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = MAIN_C.read_text(encoding="utf-8")

    def test_ui_uses_protocol_parameter_pages(self):
        for token in [
            "UI_ITEM_CURRENT",
            "UI_ITEM_TRIGGER",
            "UI_ITEM_POST_GAS",
            "UI_ITEM_ARC_TIME",
            "UI_ITEM_WORK_TYPE",
            "UI_ITEM_COUNT",
        ]:
            self.assertIn(token, self.source)

    def test_work_type_modes_match_protocol(self):
        for token in [
            "WORK_TYPE_PLATE = 0",
            "WORK_TYPE_MESH",
            "WORK_TYPE_CLEAN",
            "WORK_TYPE_GOUGE",
            '"PLATE"',
            '"MESH"',
            '"CLEAN"',
            '"GOUGE"',
            '"TYPE"',
        ]:
            self.assertIn(token, self.source)

    def test_parameter_ranges_match_protocol(self):
        for name, value in [
            ("CURRENT_MIN_A", 15),
            ("CURRENT_240V_MAX_A", 55),
            ("CURRENT_120V_MAX_A", 40),
            ("CURRENT_LIMITED_MAX_A", 30),
            ("GAS_MIN_S", 3),
            ("GAS_MAX_S", 15),
            ("ARC_MIN_S", 3),
            ("ARC_MAX_S", 15),
        ]:
            self.assertRegex(self.source, rf"#define\s+{name}\s+{value}")
        self.assertIn("Ui_CurrentMaxForType", self.source)
        self.assertIn("Ui_ClampCurrentToType", self.source)
        self.assertIn("INPUT_VOLTAGE_120V", self.source)
        self.assertIn("INPUT_VOLTAGE_240V", self.source)
        self.assertIn("g_inputVoltage == INPUT_VOLTAGE_120V", self.source)
        self.assertIn("return CURRENT_120V_MAX_A", self.source)

    def test_buttons_select_with_up_down_and_adjust_with_left_right(self):
        main_loop = re.search(r"while \(1\) \{(?P<body>.*)\n    \}", self.source, re.S)
        self.assertIsNotNone(main_loop)
        body = main_loop.group("body")
        self.assertRegex(body, r"EdgeButton_Update\(&g_upButton,\s*RdUp\).*Ui_SelectPreviousItem")
        self.assertRegex(body, r"EdgeButton_Update\(&g_downButton,\s*RdDown\).*Ui_SelectNextItem")
        self.assertRegex(body, r"RepeatButton_Update\(&g_leftRepeat,\s*RdLeft,\s*-1\)")
        self.assertRegex(body, r"RepeatButton_Update\(&g_rightRepeat,\s*RdRight,\s*1\)")
        self.assertNotIn("Debounce(RdLeft)", body)
        self.assertNotIn("Debounce(RdRight)", body)

    def test_left_right_buttons_support_hold_repeat(self):
        for token in [
            "REPEAT_DEBOUNCE_TICKS",
            "REPEAT_START_TICKS",
            "REPEAT_NEXT_TICKS",
            "RepeatButton_T",
            "RepeatButton_Update",
            "g_leftRepeat",
            "g_rightRepeat",
        ]:
            self.assertIn(token, self.source)

        repeat_body = re.search(
            r"static void RepeatButton_Update\(RepeatButton_T \*btn, uint8_t \(\*readPin\)\(void\), int8_t delta\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(repeat_body)
        body = repeat_body.group("body")
        self.assertIn("Ui_AdjustSelectedValue(delta)", body)
        self.assertIn("btn->repeatTicks", body)
        self.assertIn("REPEAT_START_TICKS", body)
        self.assertIn("REPEAT_NEXT_TICKS", body)

    def test_main_loop_never_full_refreshes_after_startup(self):
        main_loop = re.search(r"while \(1\) \{(?P<body>.*)\n    \}", self.source, re.S)
        self.assertIsNotNone(main_loop)
        self.assertNotIn("DrawFullScreen", main_loop.group("body"))
        self.assertIn("Ui_RefreshDirty", main_loop.group("body"))

    def test_independent_watchdog_is_enabled_and_fed_from_main_loop(self):
        self.assertIn('#include "apm32f0xx_iwdt.h"', self.source)
        self.assertIn('#include "apm32f0xx_rcm.h"', self.source)
        self.assertIn("#define WATCHDOG_RELOAD", self.source)
        self.assertIn("#define WATCHDOG_STARTUP_TIMEOUT", self.source)
        self.assertIn("static void Watchdog_Init(void)", self.source)
        self.assertIn("static void Watchdog_Feed(void)", self.source)
        for token in [
            "RCM_EnableLSI();",
            "RCM_ReadStatusFlag(RCM_FLAG_LSIRDY)",
            "IWDT_EnableWriteAccess();",
            "IWDT_ConfigDivider(IWDT_DIV_256);",
            "IWDT_ConfigReload(WATCHDOG_RELOAD);",
            "IWDT_Refresh();",
            "IWDT_Enable();",
        ]:
            self.assertIn(token, self.source)
        watchdog_body = re.search(
            r"static void Watchdog_Init\(void\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(watchdog_body)
        self.assertIn("timeout--", watchdog_body.group("body"))
        self.assertIn("return;", watchdog_body.group("body"))
        self.assertNotIn("while (IWDT_ReadStatusFlag", watchdog_body.group("body"))

        main_body = re.search(r"int main\(void\)\s*\{(?P<body>.*?)\n\}", self.source, re.S)
        self.assertIsNotNone(main_body)
        self.assertLess(main_body.group("body").find("Watchdog_Init();"),
                        main_body.group("body").find("LCD_Init();"))

        main_loop = re.search(r"while \(1\) \{(?P<body>.*)\n    \}", self.source, re.S)
        self.assertIsNotNone(main_loop)
        body = main_loop.group("body")
        self.assertIn("Watchdog_Feed();", body)
        self.assertGreater(body.rfind("Watchdog_Feed();"), body.rfind("Ui_RefreshDirty();"))

    def test_dirty_refresh_uses_individual_row_bits(self):
        self.assertIn("DIRTY_ROW(item)", self.source)
        self.assertIn("DIRTY_ROW_MASK", self.source)
        self.assertIn("Ui_MarkRowDirty(g_selectedItem)", self.source)

        adjust_body = re.search(
            r"static void Ui_AdjustSelectedValue\(int8_t delta\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(adjust_body)
        self.assertNotIn("DIRTY_ROWS", adjust_body.group("body"))
        self.assertIn("Ui_MarkValueDirty(g_selectedItem)", adjust_body.group("body"))
        self.assertIn("Ui_MarkRowDirty(g_selectedItem)", self.source)

        refresh_body = re.search(
            r"static void Ui_RefreshDirty\(void\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(refresh_body)
        self.assertNotIn("DrawRows()", refresh_body.group("body"))
        self.assertIn("dirty & DIRTY_ROW((UiItem_T)i)", refresh_body.group("body"))

    def test_parameter_adjust_refreshes_main_value_not_full_panel(self):
        self.assertIn("DIRTY_MAIN_VALUE", self.source)
        self.assertIn("DrawMainValueDigits", self.source)
        self.assertIn("MainValueSlotWidth", self.source)

        adjust_body = re.search(
            r"static void Ui_AdjustSelectedValue\(int8_t delta\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(adjust_body)
        body = adjust_body.group("body")
        self.assertIn("Ui_MarkValueDirty(g_selectedItem)", body)

        refresh_body = re.search(
            r"static void Ui_RefreshDirty\(void\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(refresh_body)
        refresh = refresh_body.group("body")
        self.assertIn("dirty & DIRTY_MAIN_VALUE", refresh)
        self.assertIn("DrawMainValueDigits();", refresh)

        digits_body = re.search(
            r"static void DrawMainValueDigits\(void\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(digits_body)
        self.assertIn("DrawMainValueContent(0)", digits_body.group("body"))

        content_body = re.search(
            r"static void DrawMainValueContent\(uint8_t drawUnit\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(content_body)
        content = content_body.group("body")
        self.assertIn("LCD_UI_FontHeight", content)
        self.assertIn("LCD_UI_ShowFixedCellString", content)
        self.assertIn("LCD_UI_MaxCharWidth", content)
        self.assertNotIn("LCD_Fill(slotX, y", content)
        self.assertNotIn("MAIN_Y, LCD_W", content)

    def test_current_main_value_is_centered_without_unit_width(self):
        content_body = re.search(
            r"static void DrawMainValueContent\(uint8_t drawUnit\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(content_body)
        content = content_body.group("body")
        self.assertIn("g_selectedItem == UI_ITEM_CURRENT", content)
        self.assertIn("slotX = (uint8_t)((LCD_W - slotW) / 2 - 3)", content)
        self.assertIn("unitX = (uint8_t)(LCD_W - unitW - 6)", content)
        self.assertIn("SHADOW_Y - MAIN_Y", content)

    def test_parameter_adjust_refreshes_row_value_not_full_row(self):
        self.assertIn("DIRTY_ROW_VALUE(item)", self.source)
        self.assertIn("DIRTY_ROW_VALUE_MASK", self.source)
        self.assertIn("Ui_MarkRowValueDirty(item)", self.source)
        self.assertIn("DrawRowValueDigits", self.source)

        adjust_body = re.search(
            r"static void Ui_AdjustSelectedValue\(int8_t delta\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(adjust_body)
        body = adjust_body.group("body")
        self.assertIn("Ui_MarkValueDirty(g_selectedItem)", body)

        refresh_body = re.search(
            r"static void Ui_RefreshDirty\(void\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(refresh_body)
        refresh = refresh_body.group("body")
        self.assertIn("dirty & DIRTY_ROW_VALUE_MASK", refresh)
        self.assertIn("DrawRowValueDigits((UiItem_T)i);", refresh)

        row_digits_body = re.search(
            r"static void DrawRowValueDigits\(UiItem_T item\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(row_digits_body)
        self.assertIn("DrawRowValueContent(item, 0, 0)", row_digits_body.group("body"))

        row_content_body = re.search(
            r"static void DrawRowValueContent\(UiItem_T item, uint8_t drawUnit, uint8_t clearValue\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(row_content_body)
        row_content = row_content_body.group("body")
        self.assertIn("LCD_UI_ShowFixedCellString", row_content)
        self.assertIn("LCD_UI_MaxCharWidth", row_content)
        self.assertIn("if (item == UI_ITEM_WORK_TYPE)", row_content)

    def test_five_row_layout_has_type_at_bottom(self):
        self.assertIn("#define ROW_FIRST_Y    82", self.source)
        self.assertIn("#define ROW_H          14", self.source)
        self.assertIn("#define FOOTER_Y       155", self.source)
        self.assertIn("#define FOOTER_H       5", self.source)
        self.assertIn("ROW_FIRST_Y + (uint16_t)row * ROW_H", self.source)
        self.assertRegex(self.source, r"(?s)rowLabels\[UI_ITEM_COUNT\].*\"TYPE\"")

    def test_post_gas_title_is_shortened_to_fit_header(self):
        titles = re.search(
            r"static const char \*const itemTitles\[UI_ITEM_COUNT\] = \{(?P<body>.*?)\n\};",
            self.source,
            re.S,
        )
        self.assertIsNotNone(titles)
        self.assertIn('"POST"', titles.group("body"))
        self.assertNotIn('"POST GAS"', titles.group("body"))

    def test_type_page_uses_smaller_title_and_value_fonts(self):
        title_body = re.search(
            r"static void DrawTitle\(void\)\s*\{(?P<body>.*?)\n\}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(title_body)
        self.assertIn("LCD_UI_FONT_TITLE_SMALL", title_body.group("body"))
        self.assertIn("g_selectedItem == UI_ITEM_WORK_TYPE", title_body.group("body"))

        self.assertIn("LCD_UI_FONT_MAIN_TYPE", self.source)
        self.assertIn('"GOUGE"', self.source)

    def test_old_welding_mode_power_bar_ui_removed(self):
        for obsolete in [
            "MODE_TIG",
            "g_powerLevel",
            "DrawPowerBar",
            "DrawPowerLabel",
            '"TIG"',
            '"MIG"',
            '"MMA"',
            '"PLASMA"',
        ]:
            self.assertNotIn(obsolete, self.source)


if __name__ == "__main__":
    unittest.main()
