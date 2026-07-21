import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMM_C = ROOT / "Source" / "bys_comm.c"
COMM_H = ROOT / "Include" / "bys_comm.h"
MAIN_C = ROOT / "Source" / "main.c"
INT_C = ROOT / "Source" / "apm32f0xx_int.c"
UVPROJ = ROOT / "Project" / "MDK" / "TFT_Remote.uvprojx"


class BysCommContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main_c = MAIN_C.read_text(encoding="utf-8")
        cls.int_c = INT_C.read_text(encoding="utf-8")
        cls.uvproj = UVPROJ.read_text(encoding="utf-8")
        cls.comm_c = COMM_C.read_text(encoding="utf-8") if COMM_C.exists() else ""
        cls.comm_h = COMM_H.read_text(encoding="utf-8") if COMM_H.exists() else ""

    def test_comm_module_files_exist(self):
        self.assertTrue(COMM_H.exists())
        self.assertTrue(COMM_C.exists())

    def test_project_includes_comm_source(self):
        self.assertIn("bys_comm.c", self.uvproj)

    def test_comm_protocol_constants_match_doc(self):
        for pattern in [
            r"#define\s+BYS_PKT_LEN\s+12u",
            r"#define\s+BYS_HEADER_0\s+0xAAu",
            r"#define\s+BYS_HEADER_1\s+0x55u",
            r"#define\s+BYS_TAIL_0\s+0xBBu",
            r"#define\s+BYS_TAIL_1\s+0x55u",
            r"#define\s+BYS_DEVICE_TYPE\s+0x0003u",
            r"#define\s+BYS_CMD_SET_MODE\s+0x0200u",
            r"#define\s+BYS_CMD_SET_T2T4\s+0x0300u",
            r"#define\s+BYS_CMD_SET_CURRENT\s+0x0400u",
            r"#define\s+BYS_CMD_SET_POSTGAS\s+0x0500u",
            r"#define\s+BYS_CMD_SET_ARC\s+0x0600u",
            r"#define\s+BYS_CMD_SET_UNIT\s+0x0700u",
            r"#define\s+BYS_ACK_MODE\s+0x8200u",
            r"#define\s+BYS_ACK_T2T4\s+0x8300u",
            r"#define\s+BYS_ACK_CURRENT\s+0x8400u",
            r"#define\s+BYS_ACK_POSTGAS\s+0x8500u",
            r"#define\s+BYS_ACK_ARC\s+0x8600u",
            r"#define\s+BYS_ACK_UNIT\s+0x8700u",
            r"#define\s+BYS_RSP_MODE\s+0x0082u",
            r"#define\s+BYS_RSP_T2T4\s+0x0083u",
            r"#define\s+BYS_RSP_CURRENT\s+0x0084u",
            r"#define\s+BYS_RSP_POSTGAS\s+0x0085u",
            r"#define\s+BYS_RSP_ARC\s+0x0086u",
            r"#define\s+BYS_RSP_UNIT\s+0x0087u",
            r"#define\s+BYS_RSP_ALARM\s+0x0088u",
            r"#define\s+BYS_RSP_VOLTAGE\s+0x0089u",
        ]:
            self.assertRegex(self.comm_h, pattern)

        for token in [
            "#define BYS_UART_BAUD       19200u",
            "#define BYS_UART_PORT       USART1",
            "#define BYS_UART_TX_PIN     GPIO_PIN_6",
            "#define BYS_UART_RX_PIN     GPIO_PIN_7",
        ]:
            self.assertIn(token, self.comm_c)

    def test_comm_uses_usart1_pb6_pb7_8n1(self):
        for token in [
            "RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_GPIOB)",
            "RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_USART1)",
            "GPIO_ConfigPinAF(BYS_UART_TX_PORT, BYS_UART_TX_SOURCE, BYS_UART_TX_AF)",
            "GPIO_ConfigPinAF(BYS_UART_RX_PORT, BYS_UART_RX_SOURCE, BYS_UART_RX_AF)",
            "usartCfg.baudRate = BYS_UART_BAUD",
            "usartCfg.mode = USART_MODE_TX_RX",
            "usartCfg.hardwareFlowCtrl = USART_FLOW_CTRL_NONE",
            "usartCfg.parity = USART_PARITY_NONE",
            "usartCfg.stopBits = USART_STOP_BIT_1",
            "usartCfg.wordLength = USART_WORD_LEN_8B",
            "USART_EnableInterrupt(BYS_UART_PORT, USART_INT_RXBNEIE)",
            "USART_EnableInterrupt(BYS_UART_PORT, USART_INT_ERRIE)",
            "NVIC_EnableIRQRequest(USART1_IRQn, 2)",
        ]:
            self.assertIn(token, self.comm_c)

    def test_comm_exposes_frame_handler_and_task_api(self):
        self.assertIn("typedef uint8_t (*BysComm_FrameHandler_T)", self.comm_h)
        for token in [
            "BysComm_SetFrameHandler",
            "BysComm_Init",
            "BysComm_Task",
            "BysComm_USART1IrqHandler",
            "BysComm_SysTickHandler",
            "BysComm_RequestSet",
        ]:
            self.assertIn(token, self.comm_h)
            self.assertIn(token, self.comm_c)

    def test_main_and_irq_wire_comm_layer(self):
        self.assertIn('#include "bys_comm.h"', self.main_c)
        self.assertIn("BysComm_SetFrameHandler", self.main_c)
        self.assertIn("BysComm_Init();", self.main_c)
        self.assertIn("BysComm_Task();", self.main_c)
        self.assertIn("SysTick_Config(SystemCoreClock / 1000)", self.main_c)
        self.assertIn('#include "bys_comm.h"', self.int_c)
        self.assertIn("BysComm_USART1IrqHandler();", self.int_c)
        self.assertIn("BysComm_SysTickHandler();", self.int_c)

    def test_comm_polls_queries_from_power_on(self):
        self.assertRegex(self.comm_h, r"#define\s+BYS_QUERY_COUNT\s+6u")
        self.assertRegex(self.comm_h, r"#define\s+BYS_POLL_INTERVAL_MS\s+120u")

        query_table = re.search(
            r"static const uint16_t s_query_cmds\[BYS_QUERY_COUNT\]\s*=\s*\{(?P<body>.*?)\};",
            self.comm_c,
            re.S,
        )
        self.assertIsNotNone(query_table)
        body = query_table.group("body")
        query_cmds = re.findall(r"BYS_CMD_QUERY_[A-Z0-9]+", body)
        self.assertEqual(query_cmds, [
            "BYS_CMD_QUERY_VOLTAGE",
            "BYS_CMD_QUERY_MODE",
            "BYS_CMD_QUERY_CURRENT",
            "BYS_CMD_QUERY_T2T4",
            "BYS_CMD_QUERY_POSTGAS",
            "BYS_CMD_QUERY_ARC",
        ])
        for token in [
            "BYS_CMD_QUERY_UNIT",
            "BYS_CMD_QUERY_ALARM",
        ]:
            self.assertNotIn(token, body)

    def test_each_polled_data_frame_repeats_every_120ms(self):
        self.assertRegex(
            self.comm_h,
            r"#define\s+BYS_QUERY_INTERVAL_MS\s+\(BYS_POLL_INTERVAL_MS\s*/\s*BYS_QUERY_COUNT\)",
        )

        send_query = re.search(
            r"static void BysComm_SendPolledQuery\(void\)\s*\{(?P<body>.*?)\n\}",
            self.comm_c,
            re.S,
        )
        self.assertIsNotNone(send_query)
        body = send_query.group("body")
        self.assertIn("s_nextPollMs = BysComm_Now() + BYS_QUERY_INTERVAL_MS", body)
        self.assertNotIn("s_nextPollMs = BysComm_Now() + BYS_POLL_INTERVAL_MS", body)

    def test_local_set_commands_are_queued_for_transmit(self):
        self.assertIn("BysComm_RequestSet(", self.main_c)
        self.assertIn("BYS_CMD_SET_MODE", self.main_c)
        self.assertIn("BYS_CMD_SET_T2T4", self.main_c)
        self.assertIn("BYS_CMD_SET_CURRENT", self.main_c)
        self.assertIn("BYS_CMD_SET_POSTGAS", self.main_c)
        self.assertIn("BYS_CMD_SET_ARC", self.main_c)

    def test_main_loop_does_not_block_comm_polling(self):
        self.assertNotIn("while (!f())", self.main_c)
        self.assertNotIn("volatile uint32_t d = 20000", self.main_c)

        main_loop = re.search(r"while\s*\(1\)\s*\{(?P<body>.*?)\n    \}", self.main_c, re.S)
        self.assertIsNotNone(main_loop)
        body = main_loop.group("body")
        self.assertGreaterEqual(body.count("BysComm_Task();"), 2)
        self.assertLess(body.find("BysComm_Task();"), body.find("Ui_RefreshDirty();"))
        self.assertGreater(body.rfind("BysComm_Task();"), body.find("Ui_RefreshDirty();"))

    def test_local_set_frames_pause_polling_until_200ms_after_last_operation(self):
        self.assertRegex(self.comm_h, r"#define\s+BYS_POLL_RESUME_DELAY_MS\s+200u")
        self.assertIn("static uint32_t s_pollResumeMs = 0", self.comm_c)

        request_set = re.search(
            r"uint8_t BysComm_RequestSet\(uint16_t cmd, uint16_t data\)\s*\{(?P<body>.*?)\n\}",
            self.comm_c,
            re.S,
        )
        self.assertIsNotNone(request_set)
        request_body = request_set.group("body")
        self.assertIn("BysComm_TxQueuePush(cmd, data)", request_body)
        self.assertIn("s_pollResumeMs = BysComm_Now() + BYS_POLL_RESUME_DELAY_MS", request_body)

        pump = re.search(r"static void BysComm_PumpTx\(void\)\s*\{(?P<body>.*?)\n\}", self.comm_c, re.S)
        self.assertIsNotNone(pump)
        body = pump.group("body")
        queue_branch = re.search(
            r"if\s*\(!BysComm_TxQueueEmpty\(\)\)\s*\{(?P<body>.*?)\n    \}",
            body,
            re.S,
        )
        self.assertIsNotNone(queue_branch)
        self.assertIn("s_pollResumeMs = now + BYS_POLL_RESUME_DELAY_MS", queue_branch.group("body"))
        self.assertIn("now >= s_pollResumeMs", body)
        self.assertLess(body.find("if (!BysComm_TxQueueEmpty())"), body.find("now >= s_pollResumeMs"))


if __name__ == "__main__":
    unittest.main()
