/*!
 * @file        main.c
 *
 * @brief       Welding Torch Handle Remote 0.96" TFT portrait UI (80x160)
 *              UP=S2(PB2), DOWN=S3(PB10), LEFT=S4(PB11), RIGHT=S1(PB1)
 *
 *              UP/DOWN selects parameter pages.
 *              LEFT/RIGHT adjusts the selected parameter.
 *
 * @version     V2.1.0
 * @date        2026-07-16
 */

#include "main.h"
#include "lcd_init.h"
#include "lcd.h"
#include "bys_comm.h"
#include "apm32f0xx_iwdt.h"
#include "apm32f0xx_rcm.h"

typedef enum {
    UI_ITEM_CURRENT = 0,
    UI_ITEM_TRIGGER,
    UI_ITEM_POST_GAS,
    UI_ITEM_ARC_TIME,
    UI_ITEM_WORK_TYPE,
    UI_ITEM_COUNT
} UiItem_T;

typedef enum {
    TRIGGER_2T = 0,
    TRIGGER_4T
} TriggerMode_T;

typedef enum {
    WORK_TYPE_PLATE = 0,  /* Protocol: 0x0000, steel plate */
    WORK_TYPE_MESH,       /* Protocol: 0x0001, mesh */
    WORK_TYPE_CLEAN,      /* Protocol: 0x0002, rust removal */
    WORK_TYPE_GOUGE,      /* Protocol: 0x0003, gouging */
    WORK_TYPE_COUNT
} WorkType_T;

typedef enum {
    INPUT_VOLTAGE_120V = 0,
    INPUT_VOLTAGE_240V
} InputVoltage_T;

typedef struct {
    uint8_t  isDown;
    uint16_t debounceTicks;
    uint16_t repeatTicks;
} RepeatButton_T;

typedef struct {
    uint8_t  isDown;
    uint16_t debounceTicks;
} EdgeButton_T;

#define CURRENT_MIN_A          15
#define CURRENT_240V_MAX_A     55
#define CURRENT_120V_MAX_A     40
#define CURRENT_LIMITED_MAX_A  30
#define GAS_MIN_S              3
#define GAS_MAX_S              15
#define ARC_MIN_S              3
#define ARC_MAX_S              15

#define REPEAT_DEBOUNCE_TICKS  2
#define REPEAT_START_TICKS     18
#define REPEAT_NEXT_TICKS      4

#define WATCHDOG_RELOAD        800u
#define WATCHDOG_STARTUP_TIMEOUT 50000u

#define DIRTY_TITLE    0x01
#define DIRTY_MAIN     0x02
#define DIRTY_MAIN_VALUE 0x04
#define DIRTY_ROW_BASE 0x10
#define DIRTY_ROW(item) ((uint16_t)(DIRTY_ROW_BASE << (uint8_t)(item)))
#define DIRTY_ROW_MASK  (DIRTY_ROW(UI_ITEM_CURRENT) | DIRTY_ROW(UI_ITEM_TRIGGER) | \
                         DIRTY_ROW(UI_ITEM_POST_GAS) | DIRTY_ROW(UI_ITEM_ARC_TIME) | \
                         DIRTY_ROW(UI_ITEM_WORK_TYPE))
#define DIRTY_ROW_VALUE_BASE 0x0200
#define DIRTY_ROW_VALUE(item) ((uint16_t)(DIRTY_ROW_VALUE_BASE << (uint8_t)(item)))
#define DIRTY_ROW_VALUE_MASK  (DIRTY_ROW_VALUE(UI_ITEM_CURRENT) | DIRTY_ROW_VALUE(UI_ITEM_TRIGGER) | \
                               DIRTY_ROW_VALUE(UI_ITEM_POST_GAS) | DIRTY_ROW_VALUE(UI_ITEM_ARC_TIME) | \
                               DIRTY_ROW_VALUE(UI_ITEM_WORK_TYPE))
#define DIRTY_ALL      (DIRTY_TITLE | DIRTY_MAIN | DIRTY_ROW_MASK)

/* RGB565 colors matched to the latest Figma screen. */
#define UI_BG          0x0083  /* #071118 */
#define UI_CYAN        0x055D  /* #00A9E8 */
#define UI_CYAN_LITE   0x6FBF  /* #6FF7FF */
#define UI_BORDER      0x0EFF  /* #0DDEFF */
#define UI_YELLOW      0xFF0D  /* #FFE36A */
#define UI_SHADOW      0xCC42  /* #C98A16 */
#define UI_ROW_BG      0x1167  /* #102D3B */
#define UI_MUTED       0x8D36  /* #8BA5B0 */
#define UI_VALUE       0xCEBB  /* #C9D4D8 */
#define UI_MAIN_TEXT   0x39EA  /* #3F4053 */

#define HEADER_Y       0
#define HEADER_H       18
#define MAIN_Y         20
#define MAIN_H         57
#define SHADOW_Y       68
#define SHADOW_H       9
#define ROW_FIRST_Y    82
#define ROW_H          14
#define FOOTER_Y       155
#define FOOTER_H       5

static UiItem_T      g_selectedItem = UI_ITEM_CURRENT;
static uint8_t       g_currentA     = 55;
static TriggerMode_T g_triggerMode  = TRIGGER_2T;
static uint8_t       g_postGasS     = 5;
static uint8_t       g_arcTimeS     = 3;
static WorkType_T    g_workType     = WORK_TYPE_PLATE;
static uint8_t       g_pressureUnit  = 0;
static uint8_t       g_alarmCode     = 0;
static InputVoltage_T g_inputVoltage = INPUT_VOLTAGE_240V;
static uint16_t      g_dirtyFlags   = DIRTY_ALL;
static uint8_t       g_watchdogEnabled = 0;
static EdgeButton_T  g_upButton     = {0, 0};
static EdgeButton_T  g_downButton   = {0, 0};
static RepeatButton_T g_leftRepeat  = {0, 0, 0};
static RepeatButton_T g_rightRepeat = {0, 0, 0};

static const char *const itemTitles[UI_ITEM_COUNT] = {
    "CURRENT",
    "TRIGGER",
    "POST",
    "ARC TIME",
    "TYPE"
};

static const char *const rowLabels[UI_ITEM_COUNT] = {
    "CURR",
    "MODE",
    "GAS",
    "ARC",
    "TYPE"
};

static const char *const workTypeNames[WORK_TYPE_COUNT] = {
    "PLATE",
    "MESH",
    "CLEAN",
    "GOUGE"
};

/* ====================== Button Init ====================== */
void BTN_Init(void)
{
    GPIO_Config_T cfg;
    RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_GPIOB);
    cfg.pin     = BTN_UP_PIN | BTN_DOWN_PIN | BTN_LEFT_PIN | BTN_RIGHT_PIN;
    cfg.mode    = GPIO_MODE_IN;
    cfg.outtype = GPIO_OUT_TYPE_PP;
    cfg.speed   = GPIO_SPEED_50MHz;
    cfg.pupd    = GPIO_PUPD_PU;
    GPIO_Config(BTN_UP_PORT, &cfg);
}

void SystemClock_Config(void) { /* HSI 48 MHz, done by SystemInit */ }

/* ====================== Watchdog ====================== */
static void Watchdog_Init(void)
{
    uint32_t timeout;

    RCM_EnableLSI();
    timeout = WATCHDOG_STARTUP_TIMEOUT;
    while (RCM_ReadStatusFlag(RCM_FLAG_LSIRDY) == RESET) {
        if (timeout-- == 0u) {
            return;
        }
    }

    IWDT_EnableWriteAccess();
    IWDT_ConfigDivider(IWDT_DIV_256);
    IWDT_ConfigReload(WATCHDOG_RELOAD);
    timeout = WATCHDOG_STARTUP_TIMEOUT;
    while ((IWDT_ReadStatusFlag(IWDT_FLAG_DIVU) == SET) ||
           (IWDT_ReadStatusFlag(IWDT_FLAG_CNTU) == SET)) {
        if (timeout-- == 0u) {
            return;
        }
    }

    IWDT_Refresh();
    IWDT_Enable();
    g_watchdogEnabled = 1u;
}

static void Watchdog_Feed(void)
{
    if (g_watchdogEnabled) {
        IWDT_Refresh();
    }
}

/* ====================== Button Read Helpers ====================== */
static uint8_t RdUp(void)    { return GPIO_ReadInputBit(BTN_UP_PORT,    BTN_UP_PIN);    }
static uint8_t RdDown(void)  { return GPIO_ReadInputBit(BTN_DOWN_PORT,  BTN_DOWN_PIN);  }
static uint8_t RdLeft(void)  { return GPIO_ReadInputBit(BTN_LEFT_PORT,  BTN_LEFT_PIN);  }
static uint8_t RdRight(void) { return GPIO_ReadInputBit(BTN_RIGHT_PORT, BTN_RIGHT_PIN); }

static void Ui_AdjustSelectedValue(int8_t delta);
static void Ui_SendSelectedValueToDevice(void);

static uint8_t EdgeButton_Update(EdgeButton_T *btn, uint8_t (*readPin)(void))
{
    uint8_t pressed = (readPin() == 0);

    if (!pressed) {
        btn->isDown = 0;
        btn->debounceTicks = 0;
        return 0;
    }

    if (btn->debounceTicks < REPEAT_DEBOUNCE_TICKS) {
        btn->debounceTicks++;
        return 0;
    }

    if (!btn->isDown) {
        btn->isDown = 1;
        return 1;
    }

    return 0;
}

static void RepeatButton_Update(RepeatButton_T *btn, uint8_t (*readPin)(void), int8_t delta)
{
    uint8_t pressed = (readPin() == 0);

    if (!pressed) {
        btn->isDown = 0;
        btn->debounceTicks = 0;
        btn->repeatTicks = 0;
        return;
    }

    if (btn->debounceTicks < REPEAT_DEBOUNCE_TICKS) {
        btn->debounceTicks++;
        return;
    }

    if (!btn->isDown) {
        btn->isDown = 1;
        btn->repeatTicks = REPEAT_START_TICKS;
        Ui_AdjustSelectedValue(delta);
        return;
    }

    if (btn->repeatTicks > 0) {
        btn->repeatTicks--;
        return;
    }

    btn->repeatTicks = REPEAT_NEXT_TICKS;
    Ui_AdjustSelectedValue(delta);
}

/* ====================== UI State Helpers ====================== */
static uint8_t Ui_CurrentMaxForType(void)
{
    if (g_workType == WORK_TYPE_MESH || g_workType == WORK_TYPE_CLEAN) {
        return CURRENT_LIMITED_MAX_A;
    }
    if (g_inputVoltage == INPUT_VOLTAGE_120V) {
        return CURRENT_120V_MAX_A;
    }
    return CURRENT_240V_MAX_A;
}

static uint8_t Ui_ClampCurrentToType(void)
{
    uint8_t maxA = Ui_CurrentMaxForType();
    if (g_currentA > maxA) {
        g_currentA = maxA;
        return 1;
    }
    return 0;
}

static void Ui_SendSelectedValueToDevice(void)
{
    switch (g_selectedItem) {
    case UI_ITEM_CURRENT:
        BysComm_RequestSet(BYS_CMD_SET_CURRENT, g_currentA);
        break;

    case UI_ITEM_TRIGGER:
        BysComm_RequestSet(BYS_CMD_SET_T2T4, g_triggerMode);
        break;

    case UI_ITEM_POST_GAS:
        BysComm_RequestSet(BYS_CMD_SET_POSTGAS, g_postGasS);
        break;

    case UI_ITEM_ARC_TIME:
        BysComm_RequestSet(BYS_CMD_SET_ARC, g_arcTimeS);
        break;

    case UI_ITEM_WORK_TYPE:
        BysComm_RequestSet(BYS_CMD_SET_MODE, g_workType);
        break;

    default:
        break;
    }
}

static void Ui_RemoteStatusTouched(void)
{
    volatile uint8_t shadow = (uint8_t)(g_pressureUnit ^ g_alarmCode ^ (uint8_t)g_inputVoltage);
    (void)shadow;
}

static void Ui_MarkRowDirty(UiItem_T item)
{
    g_dirtyFlags |= DIRTY_ROW(item);
}

static void Ui_MarkRowValueDirty(UiItem_T item)
{
    g_dirtyFlags |= DIRTY_ROW_VALUE(item);
}

static void Ui_MarkValueDirty(UiItem_T item)
{
    Ui_MarkRowValueDirty(item);
    if (g_selectedItem == item) {
        if (item == UI_ITEM_WORK_TYPE) {
            g_dirtyFlags |= DIRTY_MAIN;
        } else {
            g_dirtyFlags |= DIRTY_MAIN_VALUE;
        }
    }
}

static void CopyText(char *out, const char *text)
{
    while (*text != '\0') {
        *out++ = *text++;
    }
    *out = '\0';
}

static void Format2(uint8_t value, char *out)
{
    out[0] = (char)('0' + value / 10);
    out[1] = (char)('0' + value % 10);
    out[2] = '\0';
}

static void FormatCurrent(char *out)
{
    out[0] = (char)('0' + g_currentA / 10);
    out[1] = (char)('0' + g_currentA % 10);
    out[2] = '\0';
}

static void Ui_GetRowValue(UiItem_T item, char *out)
{
    switch (item) {
    case UI_ITEM_CURRENT:
        FormatCurrent(out);
        out[2] = 'A';
        out[3] = '\0';
        break;

    case UI_ITEM_TRIGGER:
        out[0] = (g_triggerMode == TRIGGER_2T) ? '2' : '4';
        out[1] = 'T';
        out[2] = '\0';
        break;

    case UI_ITEM_POST_GAS:
        Format2(g_postGasS, out);
        out[2] = 's';
        out[3] = '\0';
        break;

    case UI_ITEM_ARC_TIME:
        Format2(g_arcTimeS, out);
        out[2] = 's';
        out[3] = '\0';
        break;

    case UI_ITEM_WORK_TYPE:
    default:
        CopyText(out, workTypeNames[g_workType]);
        break;
    }
}

static void Ui_GetMainValue(char *value, char *unit)
{
    switch (g_selectedItem) {
    case UI_ITEM_CURRENT:
        FormatCurrent(value);
        unit[0] = 'A';
        unit[1] = '\0';
        break;

    case UI_ITEM_TRIGGER:
        value[0] = (g_triggerMode == TRIGGER_2T) ? '2' : '4';
        value[1] = 'T';
        value[2] = '\0';
        unit[0] = '\0';
        break;

    case UI_ITEM_POST_GAS:
        Format2(g_postGasS, value);
        unit[0] = 's';
        unit[1] = '\0';
        break;

    case UI_ITEM_ARC_TIME:
        Format2(g_arcTimeS, value);
        unit[0] = 's';
        unit[1] = '\0';
        break;

    case UI_ITEM_WORK_TYPE:
    default:
        CopyText(value, workTypeNames[g_workType]);
        unit[0] = '\0';
        break;
    }
}

static void Ui_SelectPreviousItem(void)
{
    UiItem_T oldItem = g_selectedItem;

    if (g_selectedItem == UI_ITEM_CURRENT) {
        g_selectedItem = (UiItem_T)(UI_ITEM_COUNT - 1);
    } else {
        g_selectedItem = (UiItem_T)((uint8_t)g_selectedItem - 1);
    }
    g_dirtyFlags |= DIRTY_TITLE | DIRTY_MAIN;
    Ui_MarkRowDirty(oldItem);
    Ui_MarkRowDirty(g_selectedItem);
}

static void Ui_SelectNextItem(void)
{
    UiItem_T oldItem = g_selectedItem;

    if (g_selectedItem == (UiItem_T)(UI_ITEM_COUNT - 1)) {
        g_selectedItem = UI_ITEM_CURRENT;
    } else {
        g_selectedItem = (UiItem_T)((uint8_t)g_selectedItem + 1);
    }
    g_dirtyFlags |= DIRTY_TITLE | DIRTY_MAIN;
    Ui_MarkRowDirty(oldItem);
    Ui_MarkRowDirty(g_selectedItem);
}

static void Ui_AdjustSelectedValue(int8_t delta)
{
    uint8_t maxA;
    uint8_t changed = 0;

    switch (g_selectedItem) {
    case UI_ITEM_CURRENT:
        maxA = Ui_CurrentMaxForType();
        if (delta > 0 && g_currentA < maxA) {
            g_currentA++;
            Ui_MarkValueDirty(g_selectedItem);
            changed = 1;
        } else if (delta < 0 && g_currentA > CURRENT_MIN_A) {
            g_currentA--;
            Ui_MarkValueDirty(g_selectedItem);
            changed = 1;
        }
        break;

    case UI_ITEM_TRIGGER:
        g_triggerMode = (g_triggerMode == TRIGGER_2T) ? TRIGGER_4T : TRIGGER_2T;
        Ui_MarkValueDirty(g_selectedItem);
        changed = 1;
        break;

    case UI_ITEM_POST_GAS:
        if (delta > 0 && g_postGasS < GAS_MAX_S) {
            g_postGasS++;
            Ui_MarkValueDirty(g_selectedItem);
            changed = 1;
        } else if (delta < 0 && g_postGasS > GAS_MIN_S) {
            g_postGasS--;
            Ui_MarkValueDirty(g_selectedItem);
            changed = 1;
        }
        break;

    case UI_ITEM_ARC_TIME:
        if (delta > 0 && g_arcTimeS < ARC_MAX_S) {
            g_arcTimeS++;
            Ui_MarkValueDirty(g_selectedItem);
            changed = 1;
        } else if (delta < 0 && g_arcTimeS > ARC_MIN_S) {
            g_arcTimeS--;
            Ui_MarkValueDirty(g_selectedItem);
            changed = 1;
        }
        break;

    case UI_ITEM_WORK_TYPE:
    default:
        if (delta > 0) {
            g_workType = (g_workType == (WorkType_T)(WORK_TYPE_COUNT - 1))
                       ? WORK_TYPE_PLATE
                       : (WorkType_T)((uint8_t)g_workType + 1);
        } else {
            g_workType = (g_workType == WORK_TYPE_PLATE)
                       ? (WorkType_T)(WORK_TYPE_COUNT - 1)
                       : (WorkType_T)((uint8_t)g_workType - 1);
        }
        if (Ui_ClampCurrentToType()) {
            Ui_MarkValueDirty(UI_ITEM_CURRENT);
        }
        Ui_MarkValueDirty(g_selectedItem);
        changed = 1;
        break;
    }

    if (changed) {
        if (g_selectedItem == UI_ITEM_WORK_TYPE) {
            BysComm_RequestSet(BYS_CMD_SET_MODE, g_workType);
            BysComm_RequestSet(BYS_CMD_SET_CURRENT, g_currentA);
        } else {
            Ui_SendSelectedValueToDevice();
        }
    }
}

static uint8_t Ui_HandleBysFrame(uint16_t cmd, uint16_t data)
{
    /* During active operation-frame send or the 200-ms post-edit cooldown,
     * the device may still be responding to poll queries sent before the
     * edit began.  Filter out data-bearing responses that would overwrite
     * local state with stale values and cause the seen flicker. */
    if (BysComm_IsEditing()) {
        switch (cmd) {
        case BYS_ACK_MODE:   case BYS_ACK_T2T4:
        case BYS_ACK_CURRENT: case BYS_ACK_POSTGAS:
        case BYS_ACK_ARC:    case BYS_ACK_UNIT:
        case BYS_RSP_MODE:   case BYS_RSP_T2T4:
        case BYS_RSP_CURRENT: case BYS_RSP_POSTGAS:
        case BYS_RSP_ARC:    case BYS_RSP_UNIT:
            return 1;
        default:
            break;
        }
    }

    switch (cmd) {

    case BYS_RSP_MODE:
    case BYS_ACK_MODE:
        if (data < WORK_TYPE_COUNT) {
            if (g_workType != (WorkType_T)data) {
                g_workType = (WorkType_T)data;
                Ui_MarkValueDirty(UI_ITEM_WORK_TYPE);
                if (Ui_ClampCurrentToType()) {
                    Ui_MarkValueDirty(UI_ITEM_CURRENT);
                }
            }
            return 1;
        }
        return 0;

    case BYS_RSP_T2T4:
        if (data <= 1u) {
            if (g_triggerMode != (TriggerMode_T)data) {
                g_triggerMode = (TriggerMode_T)data;
                Ui_MarkValueDirty(UI_ITEM_TRIGGER);
            }
            return 1;
        }
        return 0;

    case BYS_RSP_CURRENT:
        if (data >= CURRENT_MIN_A && data <= Ui_CurrentMaxForType()) {
            if (g_currentA != (uint8_t)data) {
                g_currentA = (uint8_t)data;
                Ui_MarkValueDirty(UI_ITEM_CURRENT);
            }
            return 1;
        }
        return 0;

    case BYS_RSP_POSTGAS:
        if (data >= GAS_MIN_S && data <= GAS_MAX_S) {
            if (g_postGasS != (uint8_t)data) {
                g_postGasS = (uint8_t)data;
                Ui_MarkValueDirty(UI_ITEM_POST_GAS);
            }
            return 1;
        }
        return 0;

    case BYS_RSP_ARC:
        if (data >= ARC_MIN_S && data <= ARC_MAX_S) {
            if (g_arcTimeS != (uint8_t)data) {
                g_arcTimeS = (uint8_t)data;
                Ui_MarkValueDirty(UI_ITEM_ARC_TIME);
            }
            return 1;
        }
        return 0;

    case BYS_RSP_UNIT:
        if (data <= 2u) {
            g_pressureUnit = (uint8_t)data;
            Ui_RemoteStatusTouched();
            return 1;
        }
        return 0;

    case BYS_RSP_ALARM:
        g_alarmCode = (uint8_t)data;
        Ui_RemoteStatusTouched();
        return 1;

    case BYS_RSP_VOLTAGE:
        if (data <= 1u) {
            if (g_inputVoltage != (InputVoltage_T)data) {
                g_inputVoltage = (InputVoltage_T)data;
                if (Ui_ClampCurrentToType()) {
                    Ui_MarkValueDirty(UI_ITEM_CURRENT);
                }
            }
            Ui_RemoteStatusTouched();
            return 1;
        }
        return 0;

    default:
        return 0;
    }
}

/* ====================== UI Drawing Helpers ====================== */
static uint8_t TextWidth(const char *text, uint8_t fontId)
{
    return LCD_UI_TextWidth(text, fontId);
}

static uint8_t MaxTextWidth(const char *const *texts, uint8_t count, uint8_t fontId)
{
    uint8_t i;
    uint8_t width;
    uint8_t maxWidth = 0;

    for (i = 0; i < count; i++) {
        width = TextWidth(texts[i], fontId);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }
    return maxWidth;
}

static void DrawCenteredString(uint16_t y, const char *text, uint8_t fontId,
                               uint16_t fc, uint16_t bc)
{
    uint8_t width = TextWidth(text, fontId);
    uint8_t x = (width >= LCD_W) ? 0 : (uint8_t)((LCD_W - width) / 2);
    LCD_UI_ShowString(x, y, text, fc, bc, fontId);
}

static void DrawStaticChrome(void)
{
    LCD_Fill(0, 0, LCD_W, LCD_H, UI_BG);
    LCD_Fill(0, HEADER_Y, LCD_W, HEADER_Y + HEADER_H, UI_CYAN);
    LCD_Fill(0, HEADER_Y, LCD_W, HEADER_Y + 2, UI_CYAN_LITE);
    LCD_Fill(0, MAIN_Y, LCD_W, MAIN_Y + MAIN_H, UI_YELLOW);
    LCD_Fill(0, SHADOW_Y, LCD_W, SHADOW_Y + SHADOW_H, UI_SHADOW);
    LCD_Fill(0, FOOTER_Y, LCD_W, FOOTER_Y + FOOTER_H, UI_CYAN);
    LCD_DrawRectangle(0, 0, LCD_W - 1, LCD_H - 1, UI_BORDER);
}

static void DrawTitle(void)
{
    uint8_t fontId = (g_selectedItem == UI_ITEM_WORK_TYPE) ? LCD_UI_FONT_TITLE_SMALL : LCD_UI_FONT_TITLE;
    uint16_t y = (g_selectedItem == UI_ITEM_WORK_TYPE) ? 4 : 3;

    LCD_Fill(1, HEADER_Y + 1, LCD_W - 1, HEADER_Y + HEADER_H, UI_CYAN);
    LCD_Fill(1, HEADER_Y + 1, LCD_W - 1, HEADER_Y + 2, UI_CYAN_LITE);
    DrawCenteredString(y, itemTitles[g_selectedItem], fontId, WHITE, UI_CYAN);
}

static uint8_t MainValueSlotWidth(uint8_t fontId)
{
    if (g_selectedItem == UI_ITEM_WORK_TYPE) {
        return MaxTextWidth(workTypeNames, WORK_TYPE_COUNT, fontId);
    }
    if (g_selectedItem == UI_ITEM_TRIGGER) {
        return (uint8_t)(LCD_UI_MaxCharWidth("0123456789T", fontId) * 2);
    }
    return (uint8_t)(LCD_UI_MaxCharWidth("0123456789", fontId) * 2);
}

static void DrawMainValueContent(uint8_t drawUnit)
{
    char value[6];
    char unit[2];
    uint8_t valueW;
    uint8_t unitW;
    uint8_t slotW;
    uint8_t totalW;
    uint8_t slotX;
    uint8_t valueX;
    uint8_t unitX;
    uint8_t fontId;
    uint8_t valueH;
    uint8_t cellW = 0;
    uint8_t cellCount = 0;
    uint16_t unitY;
    uint16_t y;

    Ui_GetMainValue(value, unit);

    fontId = (g_selectedItem == UI_ITEM_WORK_TYPE) ? LCD_UI_FONT_MAIN_TYPE : LCD_UI_FONT_MAIN_NUM;
    if (g_selectedItem == UI_ITEM_WORK_TYPE) {
        valueW = TextWidth(value, fontId);
        slotW = MainValueSlotWidth(fontId);
    } else {
        cellCount = 2;
        cellW = LCD_UI_MaxCharWidth((g_selectedItem == UI_ITEM_TRIGGER) ? "0123456789T" : "0123456789",
                                    fontId);
        valueW = (uint8_t)(cellW * cellCount);
        slotW = valueW;
    }
    unitW = (unit[0] == '\0') ? 0 : TextWidth(unit, LCD_UI_FONT_TITLE);
    valueH = LCD_UI_FontHeight(fontId);
    if (g_selectedItem == UI_ITEM_CURRENT) {
        slotX = (uint8_t)((LCD_W - slotW) / 2 - 3);
        unitX = (uint8_t)(LCD_W - unitW - 6);
        y = (uint16_t)(MAIN_Y + ((SHADOW_Y - MAIN_Y) - valueH) / 2);
        unitY = (uint16_t)(y + valueH - LCD_UI_FontHeight(LCD_UI_FONT_TITLE) - 1);
    } else {
        totalW = (uint8_t)(slotW + unitW);
        slotX = (totalW >= LCD_W) ? 0 : (uint8_t)((LCD_W - totalW) / 2);
        unitX = (uint8_t)(slotX + slotW);
        y = (uint16_t)(MAIN_Y + ((SHADOW_Y - MAIN_Y) - valueH) / 2);
        unitY = (uint16_t)(y + valueH - LCD_UI_FontHeight(LCD_UI_FONT_TITLE) - 1);
    }
    valueX = (valueW >= slotW) ? slotX : (uint8_t)(slotX + (slotW - valueW) / 2);

    if (g_selectedItem == UI_ITEM_WORK_TYPE) {
        LCD_UI_ShowString(valueX, y, value, UI_MAIN_TEXT, UI_YELLOW, fontId);
    } else {
        LCD_UI_ShowFixedCellString(valueX, y, value, cellW, cellCount,
                                   UI_MAIN_TEXT, UI_YELLOW, fontId);
    }
    if (drawUnit && unit[0] != '\0') {
        LCD_UI_ShowString(unitX, unitY, unit,
                          UI_MAIN_TEXT, UI_YELLOW, LCD_UI_FONT_TITLE);
    }
}

static void DrawMainValueDigits(void)
{
    DrawMainValueContent(0);
}

static void DrawMainValue(void)
{
    LCD_Fill(1, MAIN_Y, LCD_W - 1, MAIN_Y + MAIN_H, UI_YELLOW);
    LCD_Fill(1, SHADOW_Y, LCD_W - 1, SHADOW_Y + SHADOW_H, UI_SHADOW);
    DrawMainValueContent(1);
}

static uint8_t RowValueDigitLen(UiItem_T item)
{
    if (item == UI_ITEM_WORK_TYPE) {
        return 0;
    }
    if (item == UI_ITEM_TRIGGER) {
        return 1;
    }
    return 2;
}

static uint8_t RowValueTextWidth(const char *text)
{
    return TextWidth(text, LCD_UI_FONT_ROW);
}

static uint8_t RowValueSlotWidth(UiItem_T item)
{
    if (item == UI_ITEM_WORK_TYPE) {
        return MaxTextWidth(workTypeNames, WORK_TYPE_COUNT, LCD_UI_FONT_ROW);
    }
    if (item == UI_ITEM_TRIGGER) {
        return LCD_UI_MaxCharWidth("0123456789", LCD_UI_FONT_ROW);
    }
    return (uint8_t)(LCD_UI_MaxCharWidth("0123456789", LCD_UI_FONT_ROW) * 2);
}

static uint8_t RowValueUnitWidth(UiItem_T item)
{
    if (item == UI_ITEM_CURRENT) {
        return RowValueTextWidth("A");
    }
    if (item == UI_ITEM_TRIGGER) {
        return RowValueTextWidth("T");
    }
    if (item == UI_ITEM_POST_GAS || item == UI_ITEM_ARC_TIME) {
        return RowValueTextWidth("s");
    }
    return 0;
}

static void DrawRowValueContent(UiItem_T item, uint8_t drawUnit, uint8_t clearValue)
{
    char value[6];
    uint8_t row = (uint8_t)item;
    uint16_t y = ROW_FIRST_Y + (uint16_t)row * ROW_H;
    uint8_t active = (item == g_selectedItem);
    uint16_t bg = active ? UI_ROW_BG : UI_BG;
    uint16_t valueColor = active ? UI_YELLOW : UI_VALUE;
    uint8_t valueW;
    uint8_t slotW;
    uint8_t unitW;
    uint8_t slotX;
    uint8_t valueX;
    uint8_t digitLen;
    uint8_t cellW;
    char unitChar = '\0';
    uint8_t rightX = 74;

    Ui_GetRowValue(item, value);
    slotW = RowValueSlotWidth(item);
    unitW = RowValueUnitWidth(item);
    slotX = (uint8_t)(rightX - slotW - unitW);

    if (item == UI_ITEM_WORK_TYPE) {
        valueW = RowValueTextWidth(value);
        valueX = (valueW >= slotW) ? slotX : (uint8_t)(slotX + slotW - valueW);
        if (clearValue || !drawUnit) {
            LCD_Fill(slotX, y, rightX, (uint16_t)(y + LCD_UI_FontHeight(LCD_UI_FONT_ROW)), bg);
        }
        LCD_UI_ShowString(valueX, y, value, valueColor, bg, LCD_UI_FONT_ROW);
        return;
    }

    if (!drawUnit) {
        digitLen = RowValueDigitLen(item);
        value[digitLen] = '\0';
    } else {
        digitLen = RowValueDigitLen(item);
        unitChar = value[digitLen];
        value[digitLen] = '\0';
    }
    cellW = LCD_UI_MaxCharWidth("0123456789", LCD_UI_FONT_ROW);
    valueX = slotX;
    LCD_UI_ShowFixedCellString(valueX, y, value, cellW, digitLen,
                               valueColor, bg, LCD_UI_FONT_ROW);
    if (drawUnit && unitChar != '\0') {
        value[0] = unitChar;
        value[1] = '\0';
        LCD_UI_ShowString((uint16_t)(slotX + slotW), y, value, valueColor, bg, LCD_UI_FONT_ROW);
    }
}

static void DrawRowValueDigits(UiItem_T item)
{
    DrawRowValueContent(item, 0, 0);
}

static void DrawRow(UiItem_T item)
{
    uint8_t row = (uint8_t)item;
    uint16_t y = ROW_FIRST_Y + (uint16_t)row * ROW_H;
    uint8_t active = (item == g_selectedItem);
    uint16_t bg = active ? UI_ROW_BG : UI_BG;
    uint16_t labelColor = active ? WHITE : UI_MUTED;

    LCD_Fill(3, y - 1, 77, y + ROW_H - 1, bg);
    if (active) {
        LCD_DrawRectangle(3, y - 1, 76, y + ROW_H - 2, UI_CYAN);
    }
    LCD_UI_ShowString(6, y, rowLabels[row], labelColor, bg, LCD_UI_FONT_ROW);
    DrawRowValueContent(item, 1, 0);
}

static void DrawRows(void)
{
    uint8_t i;
    LCD_Fill(1, 78, LCD_W - 1, FOOTER_Y, UI_BG);
    for (i = 0; i < (uint8_t)UI_ITEM_COUNT; i++) {
        DrawRow((UiItem_T)i);
    }
}

static void DrawFullScreen(void)
{
    DrawStaticChrome();
    DrawTitle();
    DrawMainValue();
    DrawRows();
    g_dirtyFlags = 0;
}

static void Ui_RefreshDirty(void)
{
    uint16_t dirty = g_dirtyFlags;
    uint8_t i;

    if (dirty & DIRTY_TITLE) {
        DrawTitle();
    }
    if (dirty & DIRTY_MAIN) {
        DrawMainValue();
    } else if (dirty & DIRTY_MAIN_VALUE) {
        DrawMainValueDigits();
    }
    if (dirty & DIRTY_ROW_MASK) {
        for (i = 0; i < (uint8_t)UI_ITEM_COUNT; i++) {
            if (dirty & DIRTY_ROW((UiItem_T)i)) {
                DrawRow((UiItem_T)i);
            }
        }
    }
    if (dirty & DIRTY_ROW_VALUE_MASK) {
        for (i = 0; i < (uint8_t)UI_ITEM_COUNT; i++) {
            if (dirty & DIRTY_ROW_VALUE((UiItem_T)i)) {
                DrawRowValueDigits((UiItem_T)i);
            }
        }
    }
    g_dirtyFlags = 0;
}

/* ====================== Main ====================== */
int main(void)
{
    SystemClock_Config();
    BTN_Init();
    BysComm_SetFrameHandler(Ui_HandleBysFrame);
    BysComm_Init();
    SysTick_Config(SystemCoreClock / 1000);
    Watchdog_Init();
    LCD_Init();

    DrawFullScreen();
    Watchdog_Feed();

    while (1) {
        BysComm_Task();

        if (EdgeButton_Update(&g_upButton, RdUp)) { Ui_SelectPreviousItem(); }
        if (EdgeButton_Update(&g_downButton, RdDown)) { Ui_SelectNextItem(); }
        RepeatButton_Update(&g_leftRepeat, RdLeft, -1);
        RepeatButton_Update(&g_rightRepeat, RdRight, 1);

        Ui_RefreshDirty();
        BysComm_Task();
        Watchdog_Feed();    }
}
