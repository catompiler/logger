#include "stm32f10x.h"
#include "utils/delay.h"
#define USART_STDIO
#include "usart/usart_buf.h"
#include "spi/spi.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "rtc/rtc.h"
#include "dma/dma.h"
#include "gpio/gpio.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
//#define USE_SDCARD_FATFS_DISKIO
#include "sdcard/sdcard.h"
#include "sdcard/sdcard_diskio.h"
#define USE_ROOTFS_FATFS_DISKIO
#include "rootfs.h"
#include "hires_timer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logger.h"
#include "ain.h"
#include "osc.h"
#include "din.h"
#include "dout.h"
#include "trig.h"
#include "conf.h"
#include "dio_upd.h"
#include "storage.h"
#include "oscs.h"
#include "trends.h"
#include <time.h>
#include "fattime.h"
#include "utils/critical.h"


// UART.
// Буфер записи.
#define USART_WRITE_BUF_SIZE 256
static uint8_t usart_write_buf[USART_WRITE_BUF_SIZE];
// Буфер чтения.
#define USART_READ_BUF_SIZE 16
static uint8_t usart_read_buf[USART_READ_BUF_SIZE];
// Буферизированный UART.
static usart_buf_t usart_buf;
// Скорость USART.
#define USART_BUF_BAUD 9600

// SPI.
static spi_bus_t spi;

// Таймер таймаута SD-карты.
#define SDTIM TIM7
#define SDTIM_PSC 36000UL
#define SDTIM_MS_TO_TICKS(T) ((T) * 2)
#define SDTIM_MS_MAX 32767UL
#define SDTIM_PERIOD 65536UL

// SD-карта.
static sdcard_t sdcard;

// Скорости SPI для SD.
#define SDCARD_SPI_SPEED_HIGH_BR (0)
#define SDCARD_SPI_SPEED_LOW_BR (SPI_CR1_BR_2 | SPI_CR1_BR_1)

// SDcard fatfs.
FATFS sdcard_fatfs;

// Попытки чтения карты перед сбросом.
#define SDCARD_RETRIES 2

// Сброс карты.
#define SDCARD_REINITS 2


// SDcard Diskfs.
#define DISKFS_COUNT 1
static diskfs_t diskfs[DISKFS_COUNT];

// RTC.
static struct timeval rtc_hires_tv = {0};

// АЦП.
// Число семплов АЦП1.
#define ADC1_SAMPLES_COUNT 2
// Число семплов АЦП2.
#define ADC2_SAMPLES_COUNT 2
// Число семплов АЦП3.
#define ADC3_SAMPLES_COUNT 1
// Число семплов АЦП1 и АЦП2.
#define ADC12_SAMPLES_COUNT (ADC1_SAMPLES_COUNT + ADC2_SAMPLES_COUNT)
// Общее число семплов.
#define ADC_SAMPLES_COUNT (ADC12_SAMPLES_COUNT + ADC3_SAMPLES_COUNT)
// Число передач ПДП АЦП1 и АЦП2.
#define ADC12_DMA_TRANSFERS (ADC12_SAMPLES_COUNT * sizeof(uint16_t) / sizeof(uint32_t))
// Число передач ПДП АЦП3.
#define ADC3_DMA_TRANSFERS (ADC3_SAMPLES_COUNT * sizeof(uint16_t) / sizeof(uint16_t))
// Буфер данных.
static uint16_t adc_buffer[ADC_SAMPLES_COUNT];
// Калибровочные данные.
static uint16_t adc_cal[ADC_SAMPLES_COUNT];
// Канал DMA АЦП1 и АЦП2.
#define ADC12_DMA_CH DMA1_Channel1
// Канал DMA АЦП3..
#define ADC3_DMA_CH DMA2_Channel5
// Таймер АЦП.
#define ADC_TIM TIM1

// Цифровые входа/выхода.
// Режим порта входа.
#define DIN_MODE GPIO_MODE_IN
// Конфигурация порта входа.
#define DIN_CONF GPIO_CONF_IN_FLOATING
// Режим порта выхода.
#define DOUT_MODE GPIO_MODE_OUT_2MHz
// Конфигурация порта выхода.
#define DOUT_CONF GPIO_CONF_OUT_GP_PP
// Цифровые входа.
#define DIN_1_GPIO GPIOB
#define DIN_1_PIN GPIO_PIN_9
#define DIN_2_GPIO GPIOE
#define DIN_2_PIN GPIO_PIN_0
#define DIN_3_GPIO GPIOE
#define DIN_3_PIN GPIO_PIN_1
#define DIN_4_GPIO GPIOE
#define DIN_4_PIN GPIO_PIN_2
#define DIN_5_GPIO GPIOE
#define DIN_5_PIN GPIO_PIN_3
// Цифровые выхода.
#define DOUT_1_GPIO GPIOE
#define DOUT_1_PIN GPIO_PIN_6
#define DOUT_2_GPIO GPIOE
#define DOUT_2_PIN GPIO_PIN_5
#define DOUT_3_GPIO GPIOE
#define DOUT_3_PIN GPIO_PIN_4
#define DOUT_4_GPIO GPIOC
#define DOUT_4_PIN GPIO_PIN_13


// Приоритеты прерываний.
#define IRQ_PRIOR_MAX (8)
#define IRQ_PRIOR_HIRES_TIMER (IRQ_PRIOR_MAX - 2)
#define IRQ_PRIOR_RTC (IRQ_PRIOR_MAX - 1)
#define IRQ_PRIOR_ADC_DMA (IRQ_PRIOR_MAX + 1)
#define IRQ_PRIOR_USART_BUF (IRQ_PRIOR_MAX + 2)
#define IRQ_PRIOR_SPI (IRQ_PRIOR_MAX + 5)
#define IRQ_PRIOR_DMA1_CH4 (IRQ_PRIOR_MAX + 5) // spi2_rx
#define IRQ_PRIOR_DMA1_CH5 (IRQ_PRIOR_MAX + 5) // spi2_tx
#define IRQ_PRIOR_SD_TIM (IRQ_PRIOR_MAX + 6)



/*
 * Обработчики исключений.
 */
void HardFault_Handler(void)
{
    for(;;);
}

void MemManage_Handler(void)
{
    for(;;);
}

void BusFault_Handler(void)
{
    for(;;);
}

void UsageFault_Handler(void)
{
    for(;;);
}

/**
 * Обработчики прерываний.
 */
void USART3_IRQHandler(void)
{
    usart_buf_irq_handler(&usart_buf);
}

void SPI2_IRQHandler(void)
{
    spi_bus_irq_handler(&spi);
}

void TIM1_UP_IRQHandler(void)
{
    if(SDTIM->SR & TIM_SR_UIF){
        SDTIM->SR &= ~TIM_SR_UIF;

        sdcard_timeout(&sdcard);
    }
}

// spi2 rx.
void DMA1_Channel4_IRQHandler(void)
{
    spi_bus_dma_rx_channel_irq_handler(&spi);
}

// spi2 tx.
void DMA1_Channel5_IRQHandler(void)
{
    spi_bus_dma_tx_channel_irq_handler(&spi);
}

// Прерывание ADC DMA.
void DMA1_Channel1_IRQHandler(void)
{
    if(DMA1->ISR & DMA_ISR_TCIF1){
        DMA1->IFCR = DMA_IFCR_CTCIF1;

        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        err_t err = E_NO_ERROR;

        err = ain_process_adc_data_isr(adc_buffer, &pxHigherPriorityTaskWoken);

        if(err == E_NO_ERROR){
            portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        }
    }
}

void RTC_IRQHandler(void)
{
    rtc_interrupt_handler();
}

void TIM6_IRQHandler(void)
{
    hires_timer_irq_handler();
}

static void init_hires_timer(void)
{
    hires_timer_init(TIM6);
    hires_timer_irq_set_enabled(true);

    hires_timer_start();
}

/*
 * Функции обратного вызова.
 */
static bool spi_callback(void)
{
    return sdcard_spi_callback(&sdcard);
}

/**
 * Ремап.
 */
static void init_remap(void)
{
    // Тактирование.
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    // Disable jtag.
    AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE |
            // USART3 partial.
            AFIO_MAPR_USART3_REMAP_PARTIALREMAP;
}

/**
 * Инициализация тактирования.
 */
static void init_periph_clock(void)
{
    // GPIO.
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPEEN;
    // USART buf.
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    // SPI.
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    // DMA.
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA2EN;
    // ADC.
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC3EN;
    // ADC TIM.
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    // SD TIM.
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    // Hires TIM.
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    // PWR.
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    // BKP.
    RCC->APB1ENR |= RCC_APB1ENR_BKPEN;
}

/**
 * Инициализация прерываний.
 */
static void init_interrupts(void)
{
    NVIC_SetPriorityGrouping(0x3);

    NVIC_SetPriority(USART3_IRQn, IRQ_PRIOR_USART_BUF);
    NVIC_EnableIRQ(USART3_IRQn);

    NVIC_SetPriority(SPI2_IRQn, IRQ_PRIOR_SPI);
    NVIC_EnableIRQ(SPI2_IRQn);

    NVIC_SetPriority(TIM7_IRQn, IRQ_PRIOR_SD_TIM);
    NVIC_EnableIRQ(TIM7_IRQn);

    NVIC_SetPriority(DMA1_Channel4_IRQn, IRQ_PRIOR_DMA1_CH4);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    NVIC_SetPriority(DMA1_Channel5_IRQn, IRQ_PRIOR_DMA1_CH5);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);

    NVIC_SetPriority(RTC_IRQn, IRQ_PRIOR_RTC);
    NVIC_EnableIRQ(RTC_IRQn);

    NVIC_SetPriority(DMA1_Channel1_IRQn, IRQ_PRIOR_ADC_DMA);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    NVIC_SetPriority(TIM6_IRQn, IRQ_PRIOR_HIRES_TIMER);
    NVIC_EnableIRQ(TIM6_IRQn);

    NVIC_SetPriority(RTC_IRQn, IRQ_PRIOR_RTC);
    NVIC_EnableIRQ(RTC_IRQn);
}

/**
 * Инициализирует детектирование снижения величины питания.
 */
static void init_PDR(void)
{
    PWR->CR |= PWR_CR_PLS_2V9;
    PWR->CR |= PWR_CR_PVDE;
}

// Ключи сторожевого таймера.
// Ключ снятия защиты на запись.
#define WDT_UNPROTECT_KEY 0x5555
// Ключ установки зашиты на запись - любое значение.
#define WDT_PROTECT_KEY 0xFFFF
// Ключ перезапуска таймера.
#define WDT_RELOAD_KEY 0xAAAA
// Ключ запуска таймера.
#define WDT_START_KEY 0xCCCC

//static void init_watchdog(void)
//{
//    // Включим внутренний осциллятор 40 кГц.
//    RCC->CSR |= RCC_CSR_LSION;
//    // Подождём запуска.
//    while(!(RCC->CSR & RCC_CSR_LSIRDY));
//
//    // Разблокируем защиту на запись.
//    IWDG->KR = WDT_UNPROTECT_KEY;
//
//    // Подождём сброса бита обновления предделителя.
//    while((IWDG->SR & IWDG_SR_PVU));
//    // Запишем значение предделителя.
//    IWDG->PR = 0x0;
//
//    // Подождём сброса бита обновления загружаемого значения.
//    while((IWDG->SR & IWDG_SR_RVU));
//    // Запишем загружаемое значение.
//    IWDG->RLR = 0x64;
//
//    // Заблокируем защиту на запись.
//    IWDG->KR = WDT_PROTECT_KEY;
//
//    // Получаем время счёта таймера 10 мс.
//
//    // Запустим таймер.
//    IWDG->KR = WDT_START_KEY;
//}
//
//static bool check_watchdog_reset(void)
//{
//    return (RCC->CSR & RCC_CSR_IWDGRSTF) != 0;
//}
//
//static void clear_watchdog_reset_flag(void)
//{
//    RCC->CSR |= RCC_CSR_RMVF;
//}
//
//static void reset_watchdog(void)
//{
//    IWDG->KR = WDT_RELOAD_KEY;
//}

/**
 * Инициализация тактирования на 72 МГц.
 */
void init_rcc(void)
{
    // Предделители шин.
    // AHB - нет предделителя.
    RCC->CFGR &= ~RCC_CFGR_HPRE;
    // APB1 - делитель на 2.
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    // APB2 - нет предделителя.
    RCC->CFGR &= ~RCC_CFGR_PPRE2;
    // ADC - делитель на 6.
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;
    // USB - делитель на 1.5.
    RCC->CFGR &= ~RCC_CFGR_USBPRE;

    // ФАПЧ.
    // Источник.
    RCC->CFGR |= RCC_CFGR_PLLSRC_HSE;
    // Делитель частоты внешнего осциллятора.
    RCC->CFGR &= ~RCC_CFGR_PLLXTPRE;
    // Умножитель частоты.
    RCC->CFGR |= RCC_CFGR_PLLMULL9;

    // Переход на тактирование от PLL.
    // Включение внешнего осциллятора.
    RCC->CR |= RCC_CR_HSEON;
    // Подождём запуска внешнего осциллятора.
    while(!(RCC->CR & RCC_CR_HSERDY));
    // Запустим ФАПЧ.
    RCC->CR |= RCC_CR_PLLON;
    // Подождём запуска ФАПЧ.
    while(!(RCC->CR & RCC_CR_PLLRDY));

    // Настройка чтения флеш-памяти.
    // Запретим доступ по половине цикла тактирования.
    // Доступ по половине цикла тактирования запрещён при загрузке.
    //FLASH->ACR &= ~FLASH_ACR_HLFCYA;
    // Установим задержку чтения из флеш-памяти в 2 такта.
    FLASH->ACR |= FLASH_ACR_LATENCY_2;

    // Разрешим буфер предвыборки.
    // Буфер предвыборки включен после загрузки.
    //FLASH->ACR |= FLASH_ACR_PRFTBE;
    // Подождём включения предвыборки.
    //while(!(FLASH->ACR & FLASH_ACR_PRFTBS));

    // Перейдём на тактирование от ФАПЧ.
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    // Подождём перехода на ФАПЧ.
    while(!(RCC->CFGR & RCC_CFGR_SWS_PLL));

    // Установим значение частоты ядра.
    SystemCoreClock = 72000000;
}

static suseconds_t rtc_get_usec(void)
{
    struct timeval tv;
    struct timeval sec_tv;

    CRITICAL_ENTER();
    sec_tv.tv_sec = rtc_hires_tv.tv_sec;
    sec_tv.tv_usec = rtc_hires_tv.tv_usec;
    CRITICAL_EXIT();

    hires_timer_value(&tv);

    timersub(&tv, &sec_tv, &tv);

    return tv.tv_usec;
}

static void rtc_on_second(void)
{
    hires_timer_value(&rtc_hires_tv);
}

void init_rtc(void)
{
    rtc_init();

    if(rtc_state() == DISABLE){
        // Доступ к регистрам.
        PWR->CR |= PWR_CR_DBP;

        // Сброс бэкапного домена.
        RCC->BDCR |= RCC_BDCR_BDRST;
        __NOP();
        RCC->BDCR &= ~RCC_BDCR_BDRST;

        // Доступ к регистрам.
        PWR->CR |= PWR_CR_DBP;

        // Включение LSE.
        RCC->BDCR |= RCC_BDCR_LSEON;
        // Ожидание запуска LSE.
        while(!(RCC->BDCR & RCC_BDCR_LSERDY));

        // Выбор LSE как часов RTC.
        RCC->BDCR |= RCC_BDCR_RTCSEL_LSE;
        // Включение RTC.
        RCC->BDCR |= RCC_BDCR_RTCEN;

        rtc_synchronize();
        rtc_setup(0x7fff, NULL);

        // Запрет записи в регистры RTC & Backup Domain.
        PWR->CR &= ~PWR_CR_DBP;
    }

    rtc_set_second_callback(rtc_on_second);
    rtc_set_get_usec_callback(rtc_get_usec);
}

DWORD get_fattime(void)
{
    time_t t = time(NULL);

    return fattime_from_time(&t);
}

// 12800 Hz.
#define ADC_TIM_PSC 25
#define ADC_TIM_PERIOD 225
#define ADC_TIM_COMP_VAL 1

static void init_adc_tim(void)
{
    // Предделитель.
    ADC_TIM->PSC = ADC_TIM_PSC - 1;
    // Период.
    ADC_TIM->ARR = ADC_TIM_PERIOD - 1;
    // Обновление регистров.
    ADC_TIM->EGR = TIM_EGR_UG;
    // Настройки.
    ADC_TIM->CR1 = 0;

    // Канал сравнения 3.
    // Режим - PWM2.
    ADC_TIM->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_0;
    // Значение сравнения.
    ADC_TIM->CCR3 = ADC_TIM_COMP_VAL;
    // Разрешение канала.
    ADC_TIM->CCER |= TIM_CCER_CC3E;
    // Разрешение выхода каналов таймера.
    ADC_TIM->BDTR |= TIM_BDTR_MOE;

    // Включение счётчика.
    ADC_TIM->CR1 |= TIM_CR1_CEN;
}

static void init_adc_dma(void)
{
    dma_channel_lock(ADC12_DMA_CH);
    ADC12_DMA_CH->CMAR = (uint32_t)&adc_buffer[0];
    ADC12_DMA_CH->CPAR = (uint32_t)&ADC1->DR;
    ADC12_DMA_CH->CNDTR = ADC12_DMA_TRANSFERS;
    ADC12_DMA_CH->CCR = DMA_CCR1_PL_1 | DMA_CCR1_MSIZE_1 | DMA_CCR1_PSIZE_1 |
            DMA_CCR1_MINC | DMA_CCR1_CIRC |
            DMA_CCR1_TCIE |
            DMA_CCR1_EN;


    dma_channel_lock(ADC3_DMA_CH);
    ADC3_DMA_CH->CMAR = (uint32_t)&adc_buffer[ADC12_SAMPLES_COUNT];
    ADC3_DMA_CH->CPAR = (uint32_t)&ADC3->DR;
    ADC3_DMA_CH->CNDTR = ADC3_DMA_TRANSFERS;
    ADC3_DMA_CH->CCR = DMA_CCR1_PL_1 | DMA_CCR1_MSIZE_0 | DMA_CCR1_PSIZE_0 |
            DMA_CCR1_MINC | DMA_CCR1_CIRC |
            DMA_CCR1_EN;
}

static void init_adc_gpio(void)
{
    // A - PC5.
    gpio_init(GPIOC, GPIO_PIN_5, GPIO_MODE_IN, GPIO_CONF_IN_ANALOG);
    // B - PB0.
    gpio_init(GPIOB, GPIO_PIN_0, GPIO_MODE_IN, GPIO_CONF_IN_ANALOG);
    // C - PB1.
    gpio_init(GPIOB, GPIO_PIN_1, GPIO_MODE_IN, GPIO_CONF_IN_ANALOG);
    // DC - PC4.
    gpio_init(GPIOC, GPIO_PIN_4, GPIO_MODE_IN, GPIO_CONF_IN_ANALOG);
    // AIN0 - PC2.
    gpio_init(GPIOC, GPIO_PIN_2, GPIO_MODE_IN, GPIO_CONF_IN_ANALOG);
}

static void init_adc_cal(void)
{
    uint16_t cal_data = 0;

    // ADC1.
    ADC1->CR2 = ADC_CR2_RSTCAL | ADC_CR2_ADON;
    while(ADC1->CR2 & ADC_CR2_RSTCAL);
    ADC1->CR2 = ADC_CR2_CAL | ADC_CR2_ADON;
    while(ADC1->CR2 & ADC_CR2_CAL);
    cal_data = ADC1->DR;
    adc_cal[0] = cal_data;
    adc_cal[2] = cal_data;
    ADC1->CR2 = 0;

    // ADC2.
    ADC2->CR2 = ADC_CR2_RSTCAL | ADC_CR2_ADON;
    while(ADC2->CR2 & ADC_CR2_RSTCAL);
    ADC2->CR2 = ADC_CR2_CAL | ADC_CR2_ADON;
    while(ADC2->CR2 & ADC_CR2_CAL);
    cal_data = ADC2->DR;
    adc_cal[1] = cal_data;
    adc_cal[3] = cal_data;
    ADC2->CR2 = 0;

    // ADC3.
    ADC3->CR2 = ADC_CR2_RSTCAL | ADC_CR2_ADON;
    while(ADC3->CR2 & ADC_CR2_RSTCAL);
    ADC3->CR2 = ADC_CR2_CAL | ADC_CR2_ADON;
    while(ADC3->CR2 & ADC_CR2_CAL);
    cal_data = ADC3->DR;
    adc_cal[4] = cal_data;
    ADC3->CR2 = 0;
}

static void init_adc1(void)
{
    // Комбинированный режим - обычные каналы (одновременно).
    ADC1->CR1 = ADC_CR1_DUALMOD_2 | ADC_CR1_DUALMOD_1 |
            // Режим сканирования.
            ADC_CR1_SCAN;
    // Запуск конверсии по внешнему триггеру.
    ADC1->CR2 = ADC_CR2_EXTTRIG |
            // Триггер - Timer1 CC3 event.
            ADC_CR2_EXTSEL_1 |
            // Разрешение DMA.
            ADC_CR2_DMA |
            // Включение АЦП.
            ADC_CR2_ADON;
    // Время конверсии.
    // Канал 15.
    ADC1->SMPR1 = ADC_SMPR1_SMP15_0;
    // Канал 9.
    ADC1->SMPR2 = ADC_SMPR2_SMP9_0;
    // Последовательность конверсии.
    // 2 канала.
    ADC1->SQR1 = ADC_SQR1_L_0;
    // Первая конверсия - канал 15.
    ADC1->SQR3 = ADC_SQR3_SQ1_3 | ADC_SQR3_SQ1_2 | ADC_SQR3_SQ1_1 | ADC_SQR3_SQ1_0 |
            // Вторая конверсия - канал 9.
            ADC_SQR3_SQ2_3 | ADC_SQR3_SQ2_0;
}

static void init_adc2(void)
{
    // Комбинированный режим - обычные каналы (одновременно).
    ADC2->CR1 = ADC_CR1_DUALMOD_2 | ADC_CR1_DUALMOD_1 |
            // Режим сканирования.
            ADC_CR1_SCAN;
    // Запуск конверсии по внешнему триггеру.
    ADC2->CR2 = ADC_CR2_EXTTRIG |
            // Триггер - SWSTART.
            ADC_CR2_EXTSEL |
            // Разрешение DMA.
            ADC_CR2_DMA |
            // Включение АЦП.
            ADC_CR2_ADON;
    // Время конверсии.
    // Канал 8.
    ADC2->SMPR2 = ADC_SMPR2_SMP8_0;
    // Канал 14.
    ADC2->SMPR1 = ADC_SMPR1_SMP14_0;
    // Последовательность конверсии.
    // 2 канала.
    ADC2->SQR1 = ADC_SQR1_L_0;
    // Первая конверсия - канал 8.
    ADC2->SQR3 = ADC_SQR3_SQ1_3 |
            // Вторая конверсия - канал 14.
            ADC_SQR3_SQ2_3 | ADC_SQR3_SQ2_2 | ADC_SQR3_SQ2_1;
}

static void init_adc3(void)
{
    // Режим сканирования.
    ADC3->CR1 = ADC_CR1_SCAN;
    // Запуск конверсии по внешнему триггеру.
    ADC3->CR2 = ADC_CR2_EXTTRIG |
            // Триггер - Timer1 CC3 event.
            ADC_CR2_EXTSEL_1 |
            // Разрешение DMA.
            ADC_CR2_DMA |
            // Включение АЦП.
            ADC_CR2_ADON;
    // Время конверсии.
    // Канал 12.
    ADC3->SMPR1 = ADC_SMPR1_SMP12_0;
    // Последовательность конверсии.
    // 1 канал.
    ADC3->SQR1 = 0;
    // Первая конверсия - канал 12.
    ADC3->SQR3 = ADC_SQR3_SQ1_3 | ADC_SQR3_SQ1_2;
}

static void init_adc(void)
{
    // GPIO.
    init_adc_gpio();

    // Калибровка.
    init_adc_cal();

    // ADC1.
    init_adc1();

    // ADC2.
    init_adc2();

    // ADC3.
    init_adc3();

    // DMA.
    init_adc_dma();

    // TIM.
    init_adc_tim();
}

static void init_din_channel(size_t n, GPIO_TypeDef* gpio, gpio_pin_t pin)
{
    gpio_init(gpio, pin, DIN_MODE, DIN_CONF);
    din_channel_init(n, gpio, pin);
}

static void init_din(void)
{
    din_init();

    init_din_channel(0, DIN_1_GPIO, DIN_1_PIN);
    init_din_channel(1, DIN_2_GPIO, DIN_2_PIN);
    init_din_channel(2, DIN_3_GPIO, DIN_3_PIN);
    init_din_channel(3, DIN_4_GPIO, DIN_4_PIN);
    init_din_channel(4, DIN_5_GPIO, DIN_5_PIN);
}

static void init_dout_channel(size_t n, GPIO_TypeDef* gpio, gpio_pin_t pin)
{
    gpio_init(gpio, pin, DOUT_MODE, DOUT_CONF);
    dout_channel_init(n, gpio, pin);
}

static void init_dout(void)
{
    dout_init();

    init_dout_channel(0, DOUT_1_GPIO, DOUT_1_PIN);
    init_dout_channel(1, DOUT_2_GPIO, DOUT_2_PIN);
    init_dout_channel(2, DOUT_3_GPIO, DOUT_3_PIN);
    init_dout_channel(3, DOUT_4_GPIO, DOUT_4_PIN);
}

static void init_dio_upd(void)
{
    dio_upd_init();
}

static void init_usart_buf(void)
{
    // GPIO.
    // Tx - PC10.
    gpio_init(GPIOC, GPIO_PIN_10, GPIO_MODE_OUT_2MHz, GPIO_CONF_OUT_AF_PP);
    // Rx - PC11.
    gpio_init(GPIOC, GPIO_PIN_11, GPIO_MODE_IN, GPIO_CONF_IN_PUPD);
    gpio_set_pullup(GPIOC, GPIO_PIN_11);

    // USART.
    USART3->BRR = SystemCoreClock / USART_BUF_BAUD / 2; // APB1 -> 36 MHz.
    USART3->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    usart_buf_init_t is;
    is.usart = USART3;
    is.write_buffer = usart_write_buf;
    is.write_buffer_size = USART_WRITE_BUF_SIZE;
    is.read_buffer = usart_read_buf;
    is.read_buffer_size = USART_READ_BUF_SIZE;

    usart_buf_init(&usart_buf, &is);

    usart_setup_stdio(&usart_buf);
}

static void init_spi(void)
{
    // GPIO.
    // PB13 - SCK, PB15 - MOSI: Out AF PP.
    gpio_init(GPIOB, GPIO_PIN_13 | GPIO_PIN_15,
            GPIO_MODE_OUT_50MHz, GPIO_CONF_OUT_AF_PP);
    // PB14 - MISO: In PU.
    gpio_init(GPIOB, GPIO_PIN_14, GPIO_MODE_IN, GPIO_CONF_IN_PUPD);
    gpio_set_pullup(GPIOB, GPIO_PIN_14);

    // SPI.
    SPI2->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_MSTR |
            SDCARD_SPI_SPEED_LOW_BR;

    spi_bus_init_t is;
    is.dma_rx_channel = DMA1_Channel4;
    is.dma_tx_channel = DMA1_Channel5;
    is.spi_device = SPI2;

    spi_bus_init(&spi, &is);
    spi_bus_set_callback(&spi, spi_callback);
}

static bool sdcard_set_spi_speed(sdcard_spi_speed_t speed)
{
    if(spi_bus_busy(&spi)) return false;

    bool enabled = spi_bus_enabled(&spi);

    if(!spi_bus_set_enabled(&spi, false)) return false;

    uint32_t cr = spi.spi_device->CR1 & ~SPI_CR1_BR;

    if(speed == SDCARD_SPI_SPEED_HIGH){
        cr |= SDCARD_SPI_SPEED_HIGH_BR;
    }else{ //SDCARD_SPI_SPEED_LOW
        cr |= SDCARD_SPI_SPEED_LOW_BR;
    }

    spi.spi_device->CR1 = cr;

    if(!spi_bus_set_enabled(&spi, enabled)) return false;

    return true;
}

static void sdtim_init(void)
{
    SDTIM->CR1 = TIM_CR1_OPM;
    SDTIM->CR2 = 0;
    SDTIM->DIER = 0;
    SDTIM->PSC = SDTIM_PSC-1;
    SDTIM->ARR = SDTIM_PERIOD-1;
    SDTIM->EGR = TIM_EGR_UG;
}

void sdtim_start(uint16_t time_ms)
{
    if(time_ms > SDTIM_MS_MAX) time_ms = SDTIM_MS_MAX;
    SDTIM->ARR = SDTIM_MS_TO_TICKS(time_ms) - 1;
    SDTIM->CNT = 0;
    SDTIM->SR &= ~TIM_SR_UIF;
    SDTIM->DIER |= TIM_DIER_UIE;
    SDTIM->CR1 |= TIM_CR1_CEN;
}

void sdtim_stop(void)
{
    SDTIM->CR1 &= ~TIM_CR1_CEN;
    SDTIM->DIER &= ~TIM_DIER_UIE;
    SDTIM->SR &= ~TIM_SR_UIF;
}

#define SDCARD_TIMEOUT_INIT_TIME_MS 1000
#define SDCARD_TIMEOUT_BUSY_TIME_MS 1000
#define SDCARD_TIMEOUT_READ_TIME_MS 1000

static void sdcard_begin_timeout(sdcard_timeout_t timeout)
{
    uint16_t time_ms = 0;
    switch(timeout){
    default:
        break;
    case SDCARD_TIMEOUT_INIT:
        time_ms = SDCARD_TIMEOUT_INIT_TIME_MS;
        break;
    case SDCARD_TIMEOUT_BUSY:
        time_ms = SDCARD_TIMEOUT_BUSY_TIME_MS;
        break;
    case SDCARD_TIMEOUT_READ:
        time_ms = SDCARD_TIMEOUT_READ_TIME_MS;
        break;
    }

    sdtim_start(time_ms);
}

static void sdcard_end_timeout(void)
{
    sdtim_stop();
}

static void init_sdcard(void)
{
    // GPIO.
    // PB12 - CS.
    gpio_init(GPIOB, GPIO_PIN_12, GPIO_MODE_OUT_2MHz, GPIO_CONF_OUT_GP_PP);
    // PD9 - Card Detect: In PU.
    //gpio_init(GPIOD, GPIO_PIN_9, GPIO_MODE_IN, GPIO_CONF_IN_PUPD);
    //gpio_set_pullup(GPIOD, GPIO_PIN_9);

    sdcard_init_t is;
    is.spi = &spi;
    is.gpio_cs = GPIOB;
    is.pin_cs = GPIO_PIN_12;
    is.transfer_id = SDCARD_DEFAULT_TRANSFER_ID;
    is.set_spi_speed = sdcard_set_spi_speed;
    is.timeout_begin = sdcard_begin_timeout;
    is.timeout_end = sdcard_end_timeout;

    sdtim_init();

    sdcard_init(&sdcard, &is);
    sdcard_set_crc_enabled(&sdcard, true);

    //sdcard_setup_diskio(&sdcard, 1);
}

static void init_rootfs(void)
{
    diskfs[0].disk = &sdcard;
    diskfs[0].disk_initialize = (rootfs_disk_initialize_t)sdcard_disk_initialize;
    diskfs[0].disk_status = (rootfs_disk_status_t)sdcard_disk_status;
    diskfs[0].disk_read = (rootfs_disk_read_t)sdcard_disk_read;
    diskfs[0].disk_write = (rootfs_disk_write_t)sdcard_disk_write;
    diskfs[0].disk_ioctl = (rootfs_disk_ioctl_t)sdcard_disk_ioctl;
    diskfs[0].fatfs = &sdcard_fatfs;
    diskfs[0].retries = SDCARD_RETRIES;
    diskfs[0].reinits = SDCARD_REINITS;

    rootfs_init(diskfs, DISKFS_COUNT);
}

static void init_ain(void)
{
    ain_init();
}

static void init_osc(void)
{
    oscs_init();
}

static void init_trend(void)
{
    trends_init();
}

static void init_trig(void)
{
    trig_init();
}

static void init_conf(void)
{
    conf_init();
}

static void init_storage(void)
{
    storage_init();
}

static void init_logger(void)
{
    logger_init();
}

//static bool init_card(void)
//{
//    err_t err = E_NO_ERROR;
//    uint64_t capacity;
//
//    err = sdcard_init_card(&sdcard);
//    if(err != E_NO_ERROR){
//        printf("SD card init failed! (err: %d)\r\n", (int)err);
//        return false;
//    }
//
//    static const char* card_types_str[5] = {
//        "Unknown",
//        "MMC",
//        "SDSCv1",
//        "SDSCv2",
//        "SDHC or SDXC"
//    };
//
//    printf("SD card type: %s\r\n", card_types_str[sdcard_card_type(&sdcard)]);
//
//    capacity = sdcard_capacity(&sdcard);
//    printf("SD card size: %u Mbytes\r\n", (unsigned int)(capacity >> 20)); // Bytes -> MBytes.
//
//    printf("SD sector size: %u\r\n", (unsigned int)sdcard_sector_size(&sdcard));
//    printf("SD sectors count: %u\r\n", (unsigned int)sdcard_sectors_count(&sdcard));
//
//    printf("SD read/write block size: %u\r\n", (unsigned int)sdcard_block_len(&sdcard));
//    printf("SD erase block size: %u\r\n", (unsigned int)sdcard_erase_block_len(&sdcard));
//
//    /*err = sdcard_select(&sdcard);
//    if(err == E_NO_ERROR){
//        err = sdcard_set_crc_enabled(&sdcard, true);
//    }
//    if(err != E_NO_ERROR){
//        printf("Error change crc enabled!\r\n");
//    }*/
//
//    return true;
//}

/*
 * FreeRTOS
 * static allocation
 * functions.
 */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
        StackType_t **ppxIdleTaskStackBuffer,
        uint32_t *pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
        StackType_t **ppxTimerTaskStackBuffer,
        uint32_t *pulTimerTaskStackSize )
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#if configCHECK_FOR_STACK_OVERFLOW != 0

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pxTask;
    ( void ) pcTaskName;

    __disable_irq();

    for( ;; );
}

#endif

#if configGENERATE_RUN_TIME_STATS == 1

void initHiresCounter(void)
{
    init_hires_timer();
}

uint32_t getHiresCounterValue(void)
{
    struct timeval tv;
    hires_timer_value(&tv);

    return tv.tv_sec * 1000000 + tv.tv_usec;
}

#endif

#if configASSERT_DEFINED == 1
void vFreeRtosAssert(void)
{
    taskDISABLE_INTERRUPTS();
    for( ;; );
}
#endif


int main(void)
{
    init_PDR();
    init_rcc();
    init_remap();
    init_periph_clock();

    init_rtc();
    init_usart_buf();

    init_adc();

    init_spi();
    init_sdcard();
    init_rootfs();

    init_din();
    init_dout();
    init_dio_upd();
    init_ain();
    init_osc();
    init_trend();
    init_trig();
    init_conf();
    init_storage();
    init_logger();

    init_interrupts();

    // Start RTOS.
    vTaskStartScheduler();

    for(;;){
    }
    return 0;
}
