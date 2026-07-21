/*!
 * @file        apm32f0xx_int.c
 *
 * @brief       Interrupt service routines (weak default handlers)
 *
 * @version     V1.0.0
 *
 * @date        2026-07-15
 */

#include "apm32f0xx_int.h"
#include "main.h"
#include "bys_comm.h"

/* ========== Cortex-M0 Exception Handlers ========== */

void NMI_Handler(void)
{
    /* Non-maskable interrupt - trapped for debugging */
    while (1);
}

void HardFault_Handler(void)
{
    /* Hard fault - trapped for debugging */
    while (1);
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    BysComm_SysTickHandler();
}

/* ========== Peripheral IRQ Handlers (weak, default empty) ========== */
/* Uncomment and populate as needed for your application. */

/*
void USART1_IRQHandler(void)
{
}
*/

void USART1_IRQHandler(void)
{
    BysComm_USART1IrqHandler();
}
