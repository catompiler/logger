#include "hires_timer.h"
#include <stddef.h>
#include "utils/critical.h"


//! Тип структуры высокоточного таймера.
typedef struct _Drive_Hires_Timer {
    TIM_TypeDef* timer; //!< Периферия таймера.
    uint32_t counter; //!< Счётчик переполнений таймера.
} hires_timer_t;

//! Высокоточный таймер.
static hires_timer_t hires_timer;



static void init_timer_priph(TIM_TypeDef* TIM)
{
    TIM->CR1 = 0;
    TIM->CR2 = 0;
    TIM->DIER = 0;
    TIM->PSC = HIRES_TIMER_PRESCALER - 1;
    TIM->ARR = HIRES_TIMER_PERIOD_TICKS - 1;
    TIM->CNT = 0;
    TIM->SR  = 0;
}


err_t hires_timer_init(TIM_TypeDef* timer)
{
    if(timer == NULL) return E_NULL_POINTER;
    
    hires_timer.timer = timer;
    
    init_timer_priph(timer);
    
    return E_NO_ERROR;
}

void hires_timer_irq_handler(void)
{
    if(hires_timer.timer->SR & TIM_SR_UIF){
        hires_timer.timer->SR = ~TIM_SR_UIF;
        
        hires_timer.counter ++;
    }
}

bool hires_timer_irq_enabled(void)
{
    return hires_timer.timer->DIER & TIM_DIER_UIE;
}

void hires_timer_irq_set_enabled(bool enabled)
{
    if(enabled){
        hires_timer.timer->DIER |= TIM_DIER_UIE;
    }else{
        hires_timer.timer->DIER &= ~TIM_DIER_UIE;
    }
}

bool hires_timer_running(void)
{
    return hires_timer.timer->CR1 & TIM_CR1_CEN;
}

void hires_timer_set_running(bool running)
{
    if(running){
        hires_timer.timer->CR1 |= TIM_CR1_CEN;
    }else{
        hires_timer.timer->CR1 &= ~TIM_CR1_CEN;
    }
}

void hires_timer_start(void)
{
    hires_timer.timer->CR1 |= TIM_CR1_CEN;
}

void hires_timer_stop(void)
{
    hires_timer.timer->CR1 &= ~TIM_CR1_CEN;
}

void hires_timer_reset(void)
{
    hires_timer.timer->CNT = 0;
}

void hires_timer_value(struct timeval* tv)
{
    if(tv == NULL) return;
    
    // Сохраним флаг разрешения прерывания.
    uint16_t dier = hires_timer.timer->DIER;
    // Запретим прерывание переполнения.
    hires_timer.timer->DIER &= ~TIM_DIER_UIE;
    
    // Считаем значение счётчиков.
    uint32_t cnt_us = hires_timer.timer->CNT;
    uint32_t cnt_ms = hires_timer.counter;
    
    // Защита от переполнения.
    // Повторно считаем счётчик таймера.
    uint32_t cnt_us2 = hires_timer.timer->CNT;
    
    // Если прерывание переполнения было разрешено.
    if(dier & TIM_DIER_UIE){
        // Вновь разрешим прерывание переполнения.
        hires_timer.timer->DIER |= TIM_DIER_UIE;
    }
    
    // Если он меньше предыдущего значения - таймер переполнился.
    if(cnt_us2 < cnt_us){
        // Присвоим новое значение.
        cnt_us = cnt_us2;
        // И увеличим счётчик миллисекунд.
        cnt_ms ++;
    }
    
    tv->tv_sec = cnt_ms / 1000;
    tv->tv_usec = (cnt_ms % 1000) * 1000 + cnt_us;
}

