/*!
 * @file        bys_comm.c
 *
 * @brief       BTC550DP Ultra UART protocol transport
 *
 * @version     V1.0.1
 * @date        2026-07-17
 */

#include "bys_comm.h"
#include "apm32f0xx_gpio.h"
#include "apm32f0xx_misc.h"
#include "apm32f0xx_rcm.h"
#include "apm32f0xx_usart.h"

#define BYS_UART_PORT       USART1
#define BYS_UART_TX_PORT    GPIOB
#define BYS_UART_RX_PORT    GPIOB
#define BYS_UART_TX_PIN     GPIO_PIN_6
#define BYS_UART_RX_PIN     GPIO_PIN_7
#define BYS_UART_TX_SOURCE  GPIO_PIN_SOURCE_6
#define BYS_UART_RX_SOURCE  GPIO_PIN_SOURCE_7
#define BYS_UART_TX_AF      GPIO_AF_PIN0
#define BYS_UART_RX_AF      GPIO_AF_PIN0
#define BYS_UART_BAUD       19200u

#define BYS_RX_BUF_SIZE     96u
#define BYS_TX_QUEUE_SIZE   8u
#define BYS_REPLY_TIMEOUT_MS 35u

typedef struct {
    uint16_t cmd;
    uint16_t data;
} BysTxFrame_T;

static const uint16_t s_query_cmds[BYS_QUERY_COUNT] = {
    BYS_CMD_QUERY_VOLTAGE,
    BYS_CMD_QUERY_MODE,
    BYS_CMD_QUERY_CURRENT,
    BYS_CMD_QUERY_T2T4,
    BYS_CMD_QUERY_POSTGAS,
    BYS_CMD_QUERY_ARC,
};

static BysComm_FrameHandler_T s_frameHandler = 0;
static volatile uint8_t s_rxBuf[BYS_RX_BUF_SIZE];
static volatile uint8_t s_rxHead = 0;
static volatile uint8_t s_rxTail = 0;
static uint8_t s_frameBuf[BYS_PKT_LEN];
static uint8_t s_frameLen = 0;
static volatile uint32_t s_msTick = 0;
static uint8_t s_pollIndex = 0;
static uint32_t s_nextPollMs = 0;
static uint32_t s_replyDeadlineMs = 0;
static uint8_t s_waitingReply = 0;
static BysTxFrame_T s_txQueue[BYS_TX_QUEUE_SIZE];
static uint8_t s_txHead = 0;
static uint8_t s_txTail = 0;

static uint16_t BysComm_Read16(const uint8_t *buf)
{
    return (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
}

static void BysComm_Write16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xFFu);
    buf[1] = (uint8_t)(value >> 8);
}

static uint32_t BysComm_Now(void)
{
    return s_msTick;
}

static void BysComm_RxPush(uint8_t data)
{
    uint8_t next = (uint8_t)((s_rxHead + 1u) % BYS_RX_BUF_SIZE);

    if (next == s_rxTail) {
        s_rxTail = (uint8_t)((s_rxTail + 1u) % BYS_RX_BUF_SIZE);
    }

    s_rxBuf[s_rxHead] = data;
    s_rxHead = next;
}

static uint8_t BysComm_RxPop(uint8_t *data)
{
    if (s_rxTail == s_rxHead) {
        return 0;
    }

    *data = s_rxBuf[s_rxTail];
    s_rxTail = (uint8_t)((s_rxTail + 1u) % BYS_RX_BUF_SIZE);
    return 1;
}

static uint8_t BysComm_TxQueueFull(void)
{
    return (uint8_t)(((uint8_t)(s_txTail + 1u) % BYS_TX_QUEUE_SIZE) == s_txHead);
}

static uint8_t BysComm_TxQueueEmpty(void)
{
    return (s_txHead == s_txTail);
}

static uint8_t BysComm_TxQueuePush(uint16_t cmd, uint16_t data)
{
    uint8_t idx;

    /* Replace data if same command already queued */
    idx = s_txHead;
    while (idx != s_txTail) {
        if (s_txQueue[idx].cmd == cmd) {
            s_txQueue[idx].data = data;
            return 1;
        }
        idx = (uint8_t)((idx + 1u) % BYS_TX_QUEUE_SIZE);
    }

    /* Not found — push new; drop oldest if full */
    if (BysComm_TxQueueFull()) {
        s_txHead = (uint8_t)((s_txHead + 1u) % BYS_TX_QUEUE_SIZE);
    }

    s_txQueue[s_txTail].cmd = cmd;
    s_txQueue[s_txTail].data = data;
    s_txTail = (uint8_t)((s_txTail + 1u) % BYS_TX_QUEUE_SIZE);
    return 1;
}

static uint8_t BysComm_TxQueuePop(BysTxFrame_T *frame)
{
    if (BysComm_TxQueueEmpty()) {
        return 0;
    }

    *frame = s_txQueue[s_txHead];
    s_txHead = (uint8_t)((s_txHead + 1u) % BYS_TX_QUEUE_SIZE);
    return 1;
}

static void BysComm_SendRaw(const uint8_t *pkt)
{
    uint8_t i;

    for (i = 0; i < BYS_PKT_LEN; i++) {
        while (USART_ReadStatusFlag(BYS_UART_PORT, USART_FLAG_TXBE) == RESET);
        USART_TxData(BYS_UART_PORT, pkt[i]);
    }

    while (USART_ReadStatusFlag(BYS_UART_PORT, USART_FLAG_TXC) == RESET);
    USART_ClearStatusFlag(BYS_UART_PORT, USART_FLAG_TXC);
}

static void BysComm_SendFrame(uint16_t devType, uint16_t cmd, uint16_t data)
{
    uint8_t pkt[BYS_PKT_LEN];
    uint16_t sum;

    sum = (uint16_t)(cmd + data);
    pkt[0] = BYS_HEADER_0;
    pkt[1] = BYS_HEADER_1;
    BysComm_Write16(&pkt[2], devType);
    BysComm_Write16(&pkt[4], cmd);
    BysComm_Write16(&pkt[6], data);
    BysComm_Write16(&pkt[8], sum);
    pkt[10] = BYS_TAIL_0;
    pkt[11] = BYS_TAIL_1;

    BysComm_SendRaw(pkt);
}

static void BysComm_ClearWait(void)
{
    s_waitingReply = 0;
}

static uint8_t BysComm_IsRspCmd(uint16_t cmd)
{
    switch (cmd) {
    case BYS_RSP_MODE:
    case BYS_RSP_T2T4:
    case BYS_RSP_CURRENT:
    case BYS_RSP_POSTGAS:
    case BYS_RSP_ARC:
    case BYS_RSP_UNIT:
    case BYS_RSP_ALARM:
    case BYS_RSP_VOLTAGE:
    case BYS_ACK_MODE:
    case BYS_ACK_T2T4:
    case BYS_ACK_CURRENT:
    case BYS_ACK_POSTGAS:
    case BYS_ACK_ARC:
    case BYS_ACK_UNIT:
    case BYS_RSP_ERROR:
        return 1;
    default:
        return 0;
    }
}

static void BysComm_HandleFrame(uint16_t cmd, uint16_t data)
{
    if (!BysComm_IsRspCmd(cmd)) {
        return;
    }

    if (cmd == BYS_RSP_ERROR) {
        BysComm_ClearWait();
        return;
    }

    if (s_frameHandler != 0) {
        (void)s_frameHandler(cmd, data);
    }

    BysComm_ClearWait();
}

static void BysComm_PushFrameByte(uint8_t byte)
{
    if (s_frameLen == 0u) {
        if (byte == BYS_HEADER_0) {
            s_frameBuf[0] = byte;
            s_frameLen = 1u;
        }
        return;
    }

    if (s_frameLen == 1u) {
        if (byte == BYS_HEADER_1) {
            s_frameBuf[1] = byte;
            s_frameLen = 2u;
            return;
        }

        if (byte == BYS_HEADER_0) {
            s_frameBuf[0] = byte;
            s_frameLen = 1u;
            return;
        }

        s_frameLen = 0u;
        return;
    }

    s_frameBuf[s_frameLen++] = byte;
    if (s_frameLen == BYS_PKT_LEN) {
        uint16_t cmd;
        uint16_t data;
        uint16_t sum;

        cmd = BysComm_Read16(&s_frameBuf[4]);
        data = BysComm_Read16(&s_frameBuf[6]);
        sum = BysComm_Read16(&s_frameBuf[8]);

        if (s_frameBuf[10] == BYS_TAIL_0 &&
            s_frameBuf[11] == BYS_TAIL_1 &&
            sum == (uint16_t)(cmd + data)) {
            BysComm_HandleFrame(cmd, data);
        }

        s_frameLen = 0u;
    }
}

static void BysComm_SendPolledQuery(void)
{
    uint16_t cmd;

    cmd = s_query_cmds[s_pollIndex];
    s_pollIndex++;
    if (s_pollIndex >= BYS_QUERY_COUNT) {
        s_pollIndex = 0u;
    }

    BysComm_SendFrame(BYS_DEV_APP_ON, cmd, 0u);
    s_waitingReply = 1u;
    s_replyDeadlineMs = BysComm_Now() + BYS_REPLY_TIMEOUT_MS;
    s_nextPollMs = BysComm_Now() + BYS_QUERY_INTERVAL_MS;
}

static void BysComm_PumpTx(void)
{
    BysTxFrame_T frame;
    uint32_t now;

    now = BysComm_Now();

    if (s_waitingReply && now >= s_replyDeadlineMs) {
        s_waitingReply = 0u;
    }

    if (!BysComm_TxQueueEmpty()) {
        if (!s_waitingReply) {
            BysComm_TxQueuePop(&frame);
            BysComm_SendFrame(BYS_DEV_APP_ON, frame.cmd, frame.data);
            s_waitingReply = 1u;
            s_replyDeadlineMs = now + BYS_REPLY_TIMEOUT_MS;
        }
        return;
    }

    /* Queue just became empty → start the 200ms poll-resume timer */
    if (s_nextPollMs == 0u) {
        s_nextPollMs = now + BYS_POLL_RESUME_DELAY_MS;
    }

    if (!s_waitingReply && now >= s_nextPollMs) {
        BysComm_SendPolledQuery();
    }
}

void BysComm_SetFrameHandler(BysComm_FrameHandler_T handler)
{
    s_frameHandler = handler;
}

void BysComm_SysTickHandler(void)
{
    s_msTick++;
}

uint8_t BysComm_RequestSet(uint16_t cmd, uint16_t data)
{
    uint8_t queued;

    switch (cmd) {
    case BYS_CMD_SET_MODE:
    case BYS_CMD_SET_T2T4:
    case BYS_CMD_SET_CURRENT:
    case BYS_CMD_SET_POSTGAS:
    case BYS_CMD_SET_ARC:
    case BYS_CMD_SET_UNIT:
        s_nextPollMs = 0u;
        queued = BysComm_TxQueuePush(cmd, data);
        return queued;
    default:
        return 0u;
    }
}

void BysComm_Init(void)
{
    GPIO_Config_T gpioCfg;
    USART_Config_T usartCfg;

    s_rxHead = 0u;
    s_rxTail = 0u;
    s_frameLen = 0u;
    s_msTick = 0u;
    s_pollIndex = 0u;
    s_nextPollMs = 0u;
    s_replyDeadlineMs = 0u;
    s_waitingReply = 0u;
    s_txHead = 0u;
    s_txTail = 0u;

    RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_GPIOB);
    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_USART1);

    GPIO_ConfigPinAF(BYS_UART_TX_PORT, BYS_UART_TX_SOURCE, BYS_UART_TX_AF);
    GPIO_ConfigPinAF(BYS_UART_RX_PORT, BYS_UART_RX_SOURCE, BYS_UART_RX_AF);

    gpioCfg.mode = GPIO_MODE_AF;
    gpioCfg.outtype = GPIO_OUT_TYPE_PP;
    gpioCfg.speed = GPIO_SPEED_50MHz;
    gpioCfg.pupd = GPIO_PUPD_PU;

    gpioCfg.pin = BYS_UART_TX_PIN;
    GPIO_Config(BYS_UART_TX_PORT, &gpioCfg);

    gpioCfg.pin = BYS_UART_RX_PIN;
    GPIO_Config(BYS_UART_RX_PORT, &gpioCfg);

    usartCfg.baudRate = BYS_UART_BAUD;
    usartCfg.wordLength = USART_WORD_LEN_8B;
    usartCfg.stopBits = USART_STOP_BIT_1;
    usartCfg.parity = USART_PARITY_NONE;
    usartCfg.mode = USART_MODE_TX_RX;
    usartCfg.hardwareFlowCtrl = USART_FLOW_CTRL_NONE;
    USART_Config(BYS_UART_PORT, &usartCfg);

    USART_ClearStatusFlag(BYS_UART_PORT, USART_FLAG_TXC);
    USART_EnableInterrupt(BYS_UART_PORT, USART_INT_RXBNEIE);
    USART_EnableInterrupt(BYS_UART_PORT, USART_INT_ERRIE);
    NVIC_EnableIRQRequest(USART1_IRQn, 2);
    USART_Enable(BYS_UART_PORT);
}

void BysComm_Task(void)
{
    uint8_t byte;

    while (BysComm_RxPop(&byte)) {
        BysComm_PushFrameByte(byte);
    }

    BysComm_PumpTx();
}

uint8_t BysComm_IsEditing(void)
{
    return (uint8_t)(!BysComm_TxQueueEmpty() ||
                     s_nextPollMs == 0u);
}

void BysComm_USART1IrqHandler(void)
{
    if (USART_ReadIntFlag(BYS_UART_PORT, USART_INT_FLAG_OVRE) != RESET ||
        USART_ReadIntFlag(BYS_UART_PORT, USART_INT_FLAG_NE) != RESET ||
        USART_ReadIntFlag(BYS_UART_PORT, USART_INT_FLAG_FE) != RESET ||
        USART_ReadIntFlag(BYS_UART_PORT, USART_INT_FLAG_PE) != RESET) {
        USART_RxData(BYS_UART_PORT);
        USART_ClearIntFlag(BYS_UART_PORT,
                           USART_INT_FLAG_OVRE |
                           USART_INT_FLAG_NE |
                           USART_INT_FLAG_FE |
                           USART_INT_FLAG_PE);
    }

    if (USART_ReadIntFlag(BYS_UART_PORT, USART_INT_FLAG_RXBNE) != RESET) {
        BysComm_RxPush((uint8_t)USART_RxData(BYS_UART_PORT));
    }
}
