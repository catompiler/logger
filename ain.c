#include "ain.h"
#include <arm_math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tasks_conf.h"
#include <string.h>
#include "fir.h"
#include "q15.h"
#include "decim.h"
#include "mwin.h"
#include "hires_timer.h"
#include "osc.h"
#include "oscs.h"
#include "trends.h"


//! Размер очереди.
#define AIN_QUEUE_SIZE 4

//! Разрешение АЦП, бит.
#define ADC_BITS 12

//! Разрешение с учётом знака АЦП, бит.
#define ADC_BITS_SIGNED (ADC_BITS - 1)

//! Размер буфера для имени.
#define AIN_NAME_BUF_SIZE ((AIN_NAME_LEN) + 1)

//! Размер буфера для единицы измерения.
#define AIN_UNIT_BUF_SIZE ((AIN_UNIT_LEN) + 1)


//! Команда.
typedef struct _Ain_Queue_Data {
    uint16_t adc_data[AIN_CHANNELS_COUNT];
} ain_queue_data_t;

// Значения по-умолчанию параметров каналов.
//! Тип по-умолчанию.
#define AIN_CH_DEF_TYPE AIN_DC
//! Тип действующего значения по-умолчанию.
#define AIN_CH_DEF_EFF_TYPE AIN_AVG
//! Смещение по-умолчанию.
#define AIN_CH_DEF_OFFSET 0
//! Коэффициент АЦП по-умолчанию.
#define AIN_CH_DEF_GAIN 0x7fff
//! Коэффициент действующего значения по-умолчанию.
#define AIN_CH_DEF_EFF_GAIN 0x7fff
//! Коэффициент действующего значения по-умолчанию.
#define AIN_CH_DEF_REAL_K 0x7fff


//! Порядок КИХ-фильтра.
#define AIN_FIR_P 22
//! Число кэффициентов КИХ-фильтра.
#define AIN_FIR_N (AIN_FIR_P + 1)

static q15_t fir_coefs[AIN_FIR_N] = {
		 -72,  -74,  -66,    8,  218,  620, 1226, 1990,
		2809, 3546, 4058, 4242, 4058, 3546, 2809, 1990,
		1226,  620,  218,    8,  -66,  -74, -72
};

//! Коэффициент децимации АЦП.
#define AIN_ADC_DECIM_SCALE AIN_OVERSAMPLE_RATE

//! Размер окна для вычисления действующего значения.
#define AIN_MWIN_EFF_SIZE AIN_EFF_SAMPLES
//! Корень из размера окна действующего значения.
//#define AIN_MWIN_EFF_SIZE_SQRT 0x2D414

//! Тип канала аналоговых входов.
typedef struct _Ain_Channel {
    ain_channel_type_t type; //!< Тип канала.
    ain_channel_eff_type_t eff_type; //!< Тип действующего значения.
    uint16_t adc_offset; //!< Смещение АЦП.
    q15_t adc_gain; //!< Усиление АЦП.
    iq15_t eff_gain; //!< Усиление действующего значения.
    iq15_t real_k; //!< Коэффициент преобразования в реальную величину.
    char name[AIN_NAME_BUF_SIZE]; //!< Имя.
    char unit[AIN_UNIT_BUF_SIZE]; //!< Единица измерения.
    q15_t fir_data[AIN_FIR_N]; //!< Данные КИХ-фильтра.
    fir_t fir; //!< КИХ-фильтр.
    //decim_t decim; //!< Дециматор.
    q15_t inst_value; //!< Мгновенное значение.
    q15_t eff_win_data[AIN_MWIN_EFF_SIZE]; //!< Данные скользящего окна действующего значения.
    mwin_t eff_win; //!< Скользящее окно для вычисления действующего значения.
    q15_t eff_value; //!< Действующее значение.
    bool enabled; //!< Разрешение канала.
} ain_channel_t;


//! Тип аналоговых входов.
typedef struct _Ain {
    // Задача.
    StackType_t task_stack[AIN_STACK_SIZE]; //!< Стэк задачи.
    StaticTask_t task_buffer; //!< Буфер задачи.
    TaskHandle_t task_handle; //!< Идентификатор задачи.
    // Очередь.
    ain_queue_data_t queue_storage[AIN_QUEUE_SIZE]; //!< Данные очереди.
    StaticQueue_t queue_buffer; //!< Буфер очереди.
    QueueHandle_t queue_handle; //!< Идентификатор очереди.
    // Данные.
    decim_t ch_decim; //!< Дециматор каналов.
    ain_channel_t channels[AIN_CHANNELS_COUNT]; //!< Данные каналов.
    q15_t* fir_coefs; //!< Коэффициенты КИХ-фильтра.
    bool enabled; //!< Разрешение каналов.
} ain_t;

//! Аналоговые входа.
static ain_t ain;


static void ain_adc_task_proc(void*);

static err_t ain_init_task(void)
{
    ain.task_handle = xTaskCreateStatic(ain_adc_task_proc, "ain_task",
                        AIN_STACK_SIZE, NULL, AIN_PRIORITY, ain.task_stack, &ain.task_buffer);

    if(ain.task_handle == NULL) return E_INVALID_VALUE;


    ain.queue_handle = xQueueCreateStatic(AIN_QUEUE_SIZE, sizeof(ain_queue_data_t),
                                          (uint8_t*)ain.queue_storage, &ain.queue_buffer);

    if(ain.queue_handle == NULL) return E_INVALID_VALUE;

    vQueueAddToRegistry(ain.queue_handle, "ain_queue");

    return E_NO_ERROR;
}

static void ain_init_channels(void)
{
    size_t i;

    for(i = 0; i < AIN_CHANNELS_COUNT; i ++){
        ain_init_channel(i, AIN_CH_DEF_TYPE, AIN_CH_DEF_EFF_TYPE,
                         AIN_CH_DEF_OFFSET, AIN_CH_DEF_GAIN, AIN_CH_DEF_EFF_GAIN);
        ain_channel_set_real_k(i, AIN_CH_DEF_REAL_K);
    }
}

static void ain_reset_channels(void)
{
    size_t i;

    for(i = 0; i < AIN_CHANNELS_COUNT; i ++){
        ain_channel_reset(i);
    }
}

err_t ain_init(void)
{
    memset(&ain, 0x0, sizeof(ain_t));

    err_t err = E_NO_ERROR;

    err = ain_init_task();
    if(err != E_NO_ERROR) return err;

    ain.fir_coefs = fir_coefs;

    decim_init(&ain.ch_decim, AIN_ADC_DECIM_SCALE);

    ain_init_channels();

    return E_NO_ERROR;
}

void ain_set_enabled(bool enabled)
{
	ain.enabled = enabled;
}

bool ain_enabled(void)
{
	return ain.enabled;
}

void ain_reset(void)
{
	decim_reset(&ain.ch_decim);
	ain_reset_channels();
}

err_t ain_init_channel(size_t n, ain_channel_type_t type, ain_channel_eff_type_t eff_type,
                       uint16_t offset, q15_t inst_gain, iq15_t eff_gain)
{
    if(n >= AIN_CHANNELS_COUNT) return E_OUT_OF_RANGE;

    ain_channel_t* channel = &ain.channels[n];

    channel->type = type;
    channel->eff_type = eff_type;
    channel->adc_offset = offset;
    channel->adc_gain = inst_gain;
    channel->eff_gain = eff_gain;

    fir_init(&channel->fir, ain.fir_coefs, channel->fir_data, AIN_FIR_N);
    //decim_init(&channel->decim, AIN_ADC_DECIM_SCALE);
    mwin_init(&channel->eff_win, channel->eff_win_data, AIN_MWIN_EFF_SIZE);

    channel->inst_value = 0;

    return E_NO_ERROR;
}

err_t ain_channel_set_real_k(size_t n, iq15_t real_k)
{
    if(n >= AIN_CHANNELS_COUNT) return E_OUT_OF_RANGE;

    ain_channel_t* channel = &ain.channels[n];

    channel->real_k = real_k;

    return E_NO_ERROR;
}

iq15_t ain_channel_real_k(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    ain_channel_t* channel = &ain.channels[n];

    return channel->real_k;
}

err_t ain_channel_set_name(size_t n, const char* name)
{
    if(n >= AIN_CHANNELS_COUNT) return E_OUT_OF_RANGE;
    //if(name == NULL) return E_NULL_POINTER;

    ain_channel_t* channel = &ain.channels[n];

    if(name){
        size_t len = strlen(name);

        if(len >= AIN_NAME_LEN) len = AIN_NAME_LEN;

        memcpy(channel->name, name, len);

        channel->name[len] = '\0';
    }else{
        channel->name[0] = '\0';
    }

    return E_NO_ERROR;
}

const char* ain_channel_name(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    ain_channel_t* channel = &ain.channels[n];

    return channel->name;
}

err_t ain_channel_set_unit(size_t n, const char* unit)
{
    if(n >= AIN_CHANNELS_COUNT) return E_OUT_OF_RANGE;
    //if(unit == NULL) return E_NULL_POINTER;

    ain_channel_t* channel = &ain.channels[n];

    if(unit){
        size_t len = strlen(unit);

        if(len >= AIN_UNIT_LEN) len = AIN_UNIT_LEN;

        memcpy(channel->unit, unit, len);

        channel->unit[len] = '\0';
    }else{
        channel->unit[0] = '\0';
    }

    return E_NO_ERROR;
}

const char* ain_channel_unit(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    ain_channel_t* channel = &ain.channels[n];

    return channel->unit;
}

err_t ain_channel_set_enabled(size_t n, bool enabled)
{
	if(n >= AIN_CHANNELS_COUNT) return E_OUT_OF_RANGE;

	ain_channel_t* channel = &ain.channels[n];

	channel->enabled = enabled;

    return E_NO_ERROR;
}

bool ain_channel_enabled(size_t n)
{
	if(n >= AIN_CHANNELS_COUNT) return false;

	ain_channel_t* channel = &ain.channels[n];

	return channel->enabled;
}

err_t ain_channel_reset(size_t n)
{
	if(n >= AIN_CHANNELS_COUNT) return false;

	ain_channel_t* channel = &ain.channels[n];

	fir_reset(&channel->fir);
	mwin_reset(&channel->eff_win);

	channel->inst_value = 0;
	channel->eff_value = 0;

	return E_NO_ERROR;
}

static void ain_channel_calc_eff(ain_channel_t* channel)
{
    // Мгновенное значение.
    q15_t value = channel->inst_value;

    // Если среднеквадратичное - вычислим квадрат.
    if(channel->eff_type == AIN_RMS) value = q15_mul(value, value);
    else
    // Иначе вычислим модуль для синусоидального значения.
    if(channel->type == AIN_AC) value = q15_abs(value);

    // Добавим значение в окно.
    mwin_put(&channel->eff_win, value);

    // Сумма значений окна.
    iq15_t sum = mwin_sum(&channel->eff_win);
    size_t count = mwin_size(&channel->eff_win);

    q15_t eff_value = iq15_sat(iq15_idiv(sum, (int32_t)count));

    // Для среднеквадратичного вычислим квадратный корень.
    if(channel->eff_type == AIN_RMS){

        q15_t sqrt_value = 0;

        arm_status status = arm_sqrt_q15(eff_value, &sqrt_value);

        if(status == ARM_MATH_SUCCESS){
            eff_value = sqrt_value;
        }else{
            // DEBUG!
            //channel->eff_value = eff_value;
            //__disable_irq();
            //for(;;);
            // Пусть будет 0, корень заведомо вычислится,
            // но если вдруг...
            eff_value = 0;
        }
    }

    // Умножим на коэффициент.
    eff_value = iq15_sat(iq15_mul(eff_value, channel->eff_gain));

    // Сохраним действующее значение.
    channel->eff_value = eff_value;
}

/**
 * Вычисляет очередное мгновенное и действующее значение.
 * @param n Номер канала.
 */
static void ain_process_channel_inst_data(size_t n)
{
    ain_channel_t* channel = &ain.channels[n];

    // Если канал запрещён - ничего не делаем.
    if(!channel->enabled) return;

    // Значение выборки АЦП.
    q15_t sample = 0;

    // Получим отфильтрованные данные.
    sample = fir_calc(&channel->fir);
    //sample = decim_data(&channel->decim);

    // Сохраним мгновенное значение.
    channel->inst_value = sample;

    // Вычислим среднедействующее значение.
    ain_channel_calc_eff(channel);
}

/**
 * Обрабатывает даныне АЦП канала.
 * @param n Номер канала.
 * @param adc_data Данные АЦП.
 */
static void ain_process_channel_adc_data(size_t n, uint16_t adc_data)
{
    ain_channel_t* channel = &ain.channels[n];

    // Если канал запрещён - ничего не делаем.
    if(!channel->enabled) return;

    // Значение выборки АЦП.
    q15_t sample = 0;

    // Если канал знаковый.
    if(channel->adc_offset != 0){
        // Вычтем среднюю точку.
        sample = (q15_t)((int32_t)adc_data - (int32_t)channel->adc_offset);
        // Преобразуем в формат Q15.
        sample = QNtoM(sample, ADC_BITS_SIGNED, Q15_FRACT_BITS);
    }else{
        // Возьмём полную амплитуду.
        sample = (q15_t)adc_data;
        // Преобразуем в формат Q15.
        sample = QNtoM(sample, ADC_BITS, Q15_FRACT_BITS);
    }
    // Умножим на мгновенный кэффициент пропорциональности.
    sample = q15_mul(sample, channel->adc_gain);

    // Поместим в фильтр.
    fir_put(&channel->fir, sample);
    // Децимируем.
    //decim_put(&channel->decim, sample);
}

/**
 * Обрабатывает данные АЦП каналов.
 * @param adc_data Данные АЦП.
 */
static void ain_process_adc_data(uint16_t* adc_data)
{
    /*struct timeval tv_prev, tv_cur;
    uint32_t usec = 0;

    hires_timer_value(&tv_prev);*/

    if(!ain.enabled) return;

    size_t i;

    for(i = 0; i < AIN_CHANNELS_COUNT; i ++){
        ain_process_channel_adc_data(i, adc_data[i]);
    }

    // Децимируем значения каналов.
    decim_put(&ain.ch_decim, 0);

    // Если пропустили достаточно данных.
    if(decim_ready(&ain.ch_decim)){
        // Вычислим значения каналов.
        for(i = 0; i < AIN_CHANNELS_COUNT; i ++){
            ain_process_channel_inst_data(i);
        }

        // Записать осциллограмму.
        oscs_append();
        // Записать тренды.
        trends_append();
    }

    /*hires_timer_value(&tv_cur);

    timersub(&tv_cur, &tv_prev, &tv_cur);

    usec = tv_cur.tv_usec;

    *adc_data = usec;

    __NOP();*/
}

static void ain_adc_task_proc(void* arg)
{
    (void) arg;

    static ain_queue_data_t queue_data;

    for(;;){
        if(xQueueReceive(ain.queue_handle, &queue_data, portMAX_DELAY) == pdTRUE){
            // Process ADC data.
            ain_process_adc_data(queue_data.adc_data);
        }
    }
}

err_t ain_process_adc_data_isr(uint16_t* adc_data, BaseType_t* pxHigherPriorityTaskWoken)
{
	if(!ain.enabled) return E_NO_ERROR;

    ain_queue_data_t queue_data;

    memcpy(queue_data.adc_data, adc_data, sizeof(uint16_t) * AIN_CHANNELS_COUNT);

    if(xQueueSendToBackFromISR(ain.queue_handle, &queue_data, pxHigherPriorityTaskWoken) != pdTRUE){
        return E_OUT_OF_MEMORY;
    }

    return E_NO_ERROR;
}

q15_t ain_value_inst(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    ain_channel_t* channel = &ain.channels[n];

    return channel->inst_value;
}

q15_t ain_value(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    ain_channel_t* channel = &ain.channels[n];

    return channel->eff_value;
}

iq15_t ain_q15_to_real(size_t n, q15_t value)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    ain_channel_t* channel = &ain.channels[n];

    return iq15_mull((iq15_t)value, channel->real_k);
}

q15_t ain_real_to_q15(size_t n, iq15_t value)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    ain_channel_t* channel = &ain.channels[n];

    return iq15_divl((iq15_t)value, channel->real_k);
}

