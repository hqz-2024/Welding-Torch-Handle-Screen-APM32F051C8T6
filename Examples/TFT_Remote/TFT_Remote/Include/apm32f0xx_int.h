/*!
 * @file        apm32f0xx_int.h
 *
 * @brief       Interrupt handler declarations
 *
 * @version     V1.0.0
 *
 * @date        2026-07-15
 */

#ifndef __APM32F0XX_INT_H
#define __APM32F0XX_INT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apm32f0xx.h"

void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __APM32F0XX_INT_H */
