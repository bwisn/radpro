/*
 * Rad Pro
 * FS2011 Geiger-Müller tube interface
 *
 * (C) 2022-2023 Gissio
 *
 * License: MIT
 */

#ifdef FS2011

#include <libopencm3/cm3/nvic.h>

#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "../../gm.h"
#include "fs2011.h"

#define GM_PULSE_PERIOD 106
#define GM_PULSE_PERIOD_HIGH 53

#define GM_PULSE_TIMES_NUM 16
#define GM_PULSE_TIMES_MASK (GM_PULSE_TIMES_NUM - 1)

static struct
{
    volatile uint8_t pulsesQueueHead;
    volatile uint8_t pulsesQueueTail;
    volatile uint32_t pulsesQueue[GM_PULSE_TIMES_NUM];
} gm;

void initGM(void)
{
    // Pulse timer
    rcc_periph_clock_enable(RCC_TIM2);

    timer_generate_event(TIM2, TIM_EGR_UG);

    timer_enable_counter(TIM2);

    // High voltage generation
    rcc_periph_clock_enable(RCC_TIM3);

    gpio_mode_setup(GM_HV_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, GM_HV_PIN);
    gpio_set_output_options(GM_HV_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, GM_HV_PIN);
    gpio_set_af(GM_HV_PORT, GPIO_AF1, GM_HV_PIN);

    timer_set_period(TIM3, GM_PULSE_PERIOD - 1);
    timer_set_oc_mode(TIM3, TIM_OC1, TIM_OCM_PWM1);
    timer_set_oc_value(TIM3, TIM_OC1, GM_PULSE_PERIOD_HIGH);
    timer_enable_oc_output(TIM3, TIM_OC1);

    timer_enable_counter(TIM3);

    // GM detection
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GM_DET_PIN /*| GM_DET2_PIN*/);

    exti_select_source(EXTI6, GPIOA);
    exti_set_trigger(EXTI6, EXTI_TRIGGER_FALLING);
    exti_enable_request(EXTI6);

    nvic_enable_irq(NVIC_EXTI4_15_IRQ);
}

void exti4_15_isr(void)
{
    EXTI_PR = EXTI6; // exti_reset_request(EXTI6);

    gm.pulsesQueue[gm.pulsesQueueHead] = TIM_CNT(TIM2);
    gm.pulsesQueueHead = (gm.pulsesQueueHead + 1) & GM_PULSE_TIMES_MASK;
}

void syncGMHVPulse(void)
{
    // Assumes 'while' takes <= 4 cycles:
    while ((TIM_CNT(TIM3) & (~3)) != (20 & (~3)))
        ;
}

bool getGMPulse(uint32_t *pulseTime)
{
    if (gm.pulsesQueueHead == gm.pulsesQueueTail)
        return false;

    *pulseTime = gm.pulsesQueue[gm.pulsesQueueTail];
    gm.pulsesQueueTail = (gm.pulsesQueueTail + 1) & GM_PULSE_TIMES_MASK;

    return true;
}

#endif