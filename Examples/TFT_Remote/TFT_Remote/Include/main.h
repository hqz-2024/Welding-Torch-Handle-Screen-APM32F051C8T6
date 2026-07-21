/*!
 * @file        main.h
 *
 * @brief       Button pin definitions (APM32F051C8T6)
 *
 *              UP    = S2 = PB2
 *              DOWN  = S3 = PB10
 *              LEFT  = S4 = PB11
 *              RIGHT = S1 = PB1
 *
 *              All active LOW (button connects to GND, external pull-up)
 *
 * @version     V1.2.0
 * @date        2026-07-15
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "apm32f0xx.h"
#include "apm32f0xx_gpio.h"
#include "apm32f0xx_rcm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UP   = S2 = PB2 */
#define BTN_UP_PORT          GPIOB
#define BTN_UP_PIN           GPIO_PIN_2

/* DOWN = S3 = PB10 */
#define BTN_DOWN_PORT        GPIOB
#define BTN_DOWN_PIN         GPIO_PIN_10

/* LEFT = S4 = PB11 */
#define BTN_LEFT_PORT        GPIOB
#define BTN_LEFT_PIN         GPIO_PIN_11

/* RIGHT = S1 = PB1 */
#define BTN_RIGHT_PORT       GPIOB
#define BTN_RIGHT_PIN        GPIO_PIN_1

void BTN_Init(void);
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif
