#include "Timer.h"
#include "RCC.h"
#include "NVIC.h"
#include "stm32h743xx.h"

static volatile uint32_t g_system_tick_ms = 0U;

Timer_Status_t Timer_Init(void)
{
    uint32_t timer_clock = RCC_GetTIM2ClockHz();
    uint32_t prescaler;

    if (timer_clock < 1000000UL) return TIMER_ERROR;
    prescaler = (timer_clock / 1000000UL) - 1UL;

    RCC_EnableTIM2Clock();
    TIM2->CR1 = 0U;
    TIM2->PSC = prescaler;
    TIM2->ARR = 999U;       /* 1 MHz counter, update every 1000 counts = 1 ms. */
    TIM2->CNT = 0U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR &= ~TIM_SR_UIF;
    TIM2->DIER = TIM_DIER_UIE;

    NVIC_ConfigureIRQ(TIM2_IRQn, 6U, true);
    TIM2->CR1 = TIM_CR1_CEN;
    return TIMER_OK;
}

uint32_t Timer_GetTick(void)
{
    return g_system_tick_ms;
}

bool Timer_IsElapsed(uint32_t start_tick, uint32_t period_ms)
{
    return ((uint32_t)(Timer_GetTick() - start_tick) >= period_ms);
}

void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0U)
    {
        TIM2->SR &= ~TIM_SR_UIF;
        g_system_tick_ms++;
    }
}
