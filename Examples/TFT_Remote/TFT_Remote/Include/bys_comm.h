/*!
 * @file        bys_comm.h
 *
 * @brief       BTC550DP Ultra device communication protocol
 *
 * @version     V1.0.0
 * @date        2026-07-17
 */

#ifndef __BYS_COMM_H
#define __BYS_COMM_H

#include "apm32f0xx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BYS_PKT_LEN         12u
#define BYS_HEADER_0        0xAAu
#define BYS_HEADER_1        0x55u
#define BYS_TAIL_0          0xBBu
#define BYS_TAIL_1          0x55u

#define BYS_DEVICE_TYPE     0x0003u
#define BYS_DEV_APP_ON      0x8000u
#define BYS_DEV_APP_OFF     0x0000u

#define BYS_CMD_OTA_TRIGGER 0xFE00u
#define BYS_DATA_OTA_TRIGGER 0x00FEu

#define BYS_CMD_QUERY_MODE      0x0002u
#define BYS_CMD_QUERY_T2T4      0x0003u
#define BYS_CMD_QUERY_CURRENT   0x0004u
#define BYS_CMD_QUERY_POSTGAS   0x0005u
#define BYS_CMD_QUERY_ARC       0x0006u
#define BYS_CMD_QUERY_UNIT      0x0007u
#define BYS_CMD_QUERY_ALARM     0x0008u
#define BYS_CMD_QUERY_VOLTAGE   0x0009u
#define BYS_QUERY_COUNT         6u

#define BYS_CMD_SET_MODE        0x0200u
#define BYS_CMD_SET_T2T4        0x0300u
#define BYS_CMD_SET_CURRENT     0x0400u
#define BYS_CMD_SET_POSTGAS     0x0500u
#define BYS_CMD_SET_ARC         0x0600u
#define BYS_CMD_SET_UNIT        0x0700u

#define BYS_RSP_ERROR           0x8100u

#define BYS_ACK_MODE            0x8200u
#define BYS_ACK_T2T4            0x8300u
#define BYS_ACK_CURRENT         0x8400u
#define BYS_ACK_POSTGAS         0x8500u
#define BYS_ACK_ARC             0x8600u
#define BYS_ACK_UNIT            0x8700u

#define BYS_RSP_MODE            0x0082u
#define BYS_RSP_T2T4            0x0083u
#define BYS_RSP_CURRENT         0x0084u
#define BYS_RSP_POSTGAS         0x0085u
#define BYS_RSP_ARC             0x0086u
#define BYS_RSP_UNIT            0x0087u
#define BYS_RSP_ALARM           0x0088u
#define BYS_RSP_VOLTAGE         0x0089u

#define BYS_ERR_REJECTED        0x0001u
#define BYS_POLL_INTERVAL_MS    120u
#define BYS_QUERY_INTERVAL_MS   (BYS_POLL_INTERVAL_MS / BYS_QUERY_COUNT)
#define BYS_POLL_RESUME_DELAY_MS 200u

typedef uint8_t (*BysComm_FrameHandler_T)(uint16_t cmd, uint16_t data);

void BysComm_SetFrameHandler(BysComm_FrameHandler_T handler);
void BysComm_SysTickHandler(void);
uint8_t BysComm_RequestSet(uint16_t cmd, uint16_t data);
uint8_t BysComm_IsEditing(void);
void BysComm_Init(void);
void BysComm_Task(void);
void BysComm_USART1IrqHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __BYS_COMM_H */
