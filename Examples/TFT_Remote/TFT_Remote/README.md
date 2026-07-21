# 焊接手柄遥控屏幕固件

基于 **APM32F051C8T6**（ARM Cortex-M0+, 64KB Flash, 8KB RAM）的焊机手柄遥控屏幕固件。通过 RS485 总线与焊机主机通信，在 0.96 英寸 TFT LCD 上实时显示焊接参数，并支持 4 键导航调节。

## 硬件平台

| 组件 | 规格 |
|------|------|
| **主控** | APM32F051C8T6 (LQFP48, Cortex-M0+, 48 MHz) |
| **显示屏** | 0.96" TFT IPS LCD, ST7735S 驱动, 80×160 竖屏 |
| **通信** | USART1 通过 MS2548 RS485 收发器, 19200 bps |
| **按键** | 4 个独立按键 (上/下/左/右), 低电平有效 |
| **调试** | SWD (PA13=SWDIO, PA14=SWCLK) |

### 引脚分配

| 功能 | 引脚 | 说明 |
|------|------|------|
| LCD SCK | PB13 | SPI2 时钟 |
| LCD MOSI | PB15 | SPI2 数据 |
| LCD DC | PB12 | 数据/命令选择 |
| LCD RES | PB14 | 复位 |
| LCD BLK | PA8 | 背光控制 |
| RS485 TX | PB6 | USART1 发送 |
| RS485 RX | PB7 | USART1 接收 |
| BTN UP | PB2 | 上一个参数 |
| BTN DOWN | PB10 | 下一个参数 |
| BTN LEFT | PB11 | 减小参数值 |
| BTN RIGHT | PB1 | 增大参数值 |

## 编译与烧录

### 环境要求

- **Keil MDK-ARM V5.x** + Geehy APM32F0xx DFP 包 (`Package/Geehy.APM32F0xx_DFP.1.1.3.pack`)
- SWD 调试器 (J-Link / CMSIS-DAP / ST-Link)

### 编译

1. 打开 `Project/MDK/TFT_Remote.uvprojx`
2. 选择构建目标 **APM32F030**（APM32F051C8 与该目标共用配置）
3. `F7` 编译，`F8` 烧录

### 烧录

通过 SWD 下载至芯片。建议使用 J-Link 或 CMSIS-DAP，连接 PA13(SWDIO) + PA14(SWCLK) + GND + 3.3V。

## 工程结构

```
Examples/TFT_Remote/TFT_Remote/
├── Include/
│   ├── main.h              # 按键引脚定义 + 时钟配置声明
│   ├── apm32f0xx_int.h     # 中断处理程序声明
│   └── bys_comm.h          # BYS 通信协议常量与 API
├── Source/
│   ├── main.c              # 主入口 (按键处理 + UI 绘制 + 看门狗)
│   ├── apm32f0xx_int.c     # 中断处理实现 (USART1 + SysTick)
│   └── bys_comm.c          # UART 协议栈 (帧解析/轮询/设定值队列)
├── HARDWARE/LCD/
│   ├── lcd.c / lcd.h       # 绘图 API (线/矩形/圆/文字/缩放文字)
│   ├── lcd_init.c / lcd_init.h  # ST7735S 初始化 + 硬件 SPI2 配置
│   ├── lcdfont.h           # ASCII 位图字体 (6×12, 8×16, 12×24)
│   └── ui_font_inter.h     # Inter 矢量字体字模 (5 种变体)
├── Project/MDK/            # Keil MDK 工程文件
├── tests/                  # Python 合约测试
├── tools/
│   └── generate_inter_font.py  # TTF → C 字模生成脚本
└── readme.txt              # 原始项目说明 (中文)
```

## 软件架构

### 初始化流程

```
Reset_Handler → SystemInit() → main()
                                  ├── SystemClock_Config()     (HSI 48 MHz)
                                  ├── BTN_Init()               (GPIOB 输入 + 上拉)
                                  ├── BysComm_Init()           (USART1, 19200, 中断驱动)
                                  ├── SysTick_Config()         (1ms 时基)
                                  ├── Watchdog_Init()          (IWDT, ~800ms 超时)
                                  ├── LCD_Init()               (SPI2 + ST7735S 寄存器)
                                  └── while(1):
                                        ├── BysComm_Task()     (帧收发)
                                        ├── 按键轮询 (边沿/重复)
                                        ├── Ui_RefreshDirty()  (增量重绘)
                                        └── Watchdog_Feed()
```

### 通信协议 (BTC550DP Ultra)

12 字节二进制帧格式:

| 字节 | 0 | 1 | 2-3 | 4-5 | 6-7 | 8-9 | 10 | 11 |
|------|---|---|-----|-----|-----|-----|----|----|
| 含义 | 帧头 `0xAA` | 帧头 `0x55` | 设备类型 | 命令 | 数据 | 校验和 | 帧尾 `0xBB` | 帧尾 `0x55` |

- **校验和** = 命令字段 + 数据字段（2 字节，忽略溢出）
- 主机每 120 ms 轮询 6 个查询项（电压、模式、电流、T2T4、后气、电弧时间）
- 设定值命令队列防重复：相同命令原地替换
- 编辑后 200 ms 冷却期防止轮询覆盖本地修改
- 35 ms 回复超时，超时后自动处理队列中的下一帧

### UI 参数

通过上/下键选择，左/右键调节数值：

| 参数 | 字样 | 范围 |
|------|------|------|
| **CURRENT** | CURRENT | 钢板/刨削: 15~55A；网格/清洗: 15~30A (120V 上限 40A) |
| **TRIGGER** | 2T / 4T | 焊接模式切换 |
| **POST GAS** | POST GAS | 3~15 s |
| **ARC TIME** | ARC | 3~15 s |
| **TYPE** | - | PLATE(钢板) / MESH(网格) / CLEAN(清洗) / GOUGE(刨削) |

### 增量刷新机制

使用位掩码脏标志（`DIRTY_TITLE`, `DIRTY_MAIN`, `DIRTY_MAIN_VALUE`, `DIRTY_ROW(x)`, `DIRTY_ROW_VALUE(x)`），仅重绘发生变化的 UI 区域，避免全屏重绘造成的闪烁。

## 字体系统

三套字体技术共存：

1. **ASCII 位图** (`lcdfont.h`) — 6×12 / 8×16 / 12×24 像素等宽点阵
2. **Inter UI 字体** (`ui_font_inter.h`) — 从 [Inter](https://github.com/rsms/inter) TTF 提取，5 种变体:
   - `ROW` 10px Medium — 参数行标签
   - `TITLE_SMALL` 14px SemiBold — 小标题
   - `TITLE` 16px SemiBold — 大标题
   - `MAIN_TYPE` 20px SemiBold — 工作类型
   - `MAIN_NUM` 32px SemiBold — 主电流数值
3. **生成工具** (`tools/generate_inter_font.py`) — 使用 PIL + FreeType 将任意 TTF 字体光栅化为 C 字形数组

## 依赖

- **APM32F0xx StdPeriphDriver**: GPIO, SPI, USART, IWDT, RCM, MISC, SYSCFG
- **CMSIS-Core (Cortex-M0+)**
- **无 RTOS** — 裸机 while(1) 主循环，中断驱动 USART 收发

## 测试

[`tests/`](tests/) 目录包含 4 个 Python 合约测试 (`unittest`)：

| 文件 | 验证内容 |
|------|---------|
| `test_bys_comm_contract.py` | 协议常量、查询/设定值命令、USART1 中断注册 |
| `test_ui_contract.py` | 5 个参数页面、工作类型映射、数值范围 |
| `test_lcd_spi_contract.py` | SPI2 硬件主模式、24 MHz 配置、字节发送流程 |
| `test_inter_font_contract.py` | 5 种 Inter 字体变体、`LCD_UI_*` API |

运行: `python -m pytest tests/` 或 `python -m unittest discover tests/`

## SDK 版本

基于 **Geehy APM32F0xx SDK V1.8.6**，支持 APM32F030/F051/F072/F091 系列芯片。
