#include "osc.h"
#include "decim.h"
#include "avg.h"
#include "maj.h"
#include "ain.h"
#include "din.h"
#include <string.h>


//! Резервные семплы для компенсации ошибок вычислений размера.
#define OSC_RESERVED_SAMPLES (OSC_COUNT_MAX)

//! Общее число семплов в буфере.
#define OSC_TOTAL_SAMPLES_MAX (OSC_SAMPLES_MAX + OSC_RESERVED_SAMPLES)

//! Минимальное число семплов для канала (если вычисленный размер оказывается равен нулю).
#define OSC_SAMPLES_MIN 1

//! Минимальный размер буфера для канала (если вычисленный размер оказывается равен нулю).
#define OSC_BUF_SIZE_MIN 1

//! Число цифровых сигналов в одном семпле.
#define OSC_BITS_PER_SAMPLE (sizeof(osc_value_t) * 8)

// Порог значения цифрового входа.
//#define OSC_DIN_THRESHOLD Q15(0.5)


//! Структура канала осциллограммы.
typedef struct _Osc_Channel {
    osc_src_t src; //!< Источник данных.
    osc_type_t type; //!< Тип значения.
    osc_src_type_t src_type; //!< Тип - мгновенное или действующее.
    size_t src_channel; //!< Номер канала источника данных.
    union {
        avg_t avg; //!< Усреднение.
        maj_t maj; //!< Мажоритар.
    };
    osc_value_t* data; //!< Данные.
    size_t size; //!< Размер данных в единицах буфера.
    bool enabled; //!< Разрешение канала.
} osc_channel_t;

//! Пул данных осциллограмм.
typedef struct _Osc_Pool {
    osc_value_t data[OSC_TOTAL_SAMPLES_MAX]; //!< Буфер данных пула.
    size_t index; //!< Индекс свободного элемента.
} osc_pool_t;

//! Структура осциллограмм.
typedef struct _Osc {
    osc_channel_t channels[OSC_COUNT_MAX]; //!< Каналы осциллограммы.
    size_t used_map[OSC_COUNT_MAX]; //!< Индексы используемых каналов.
    size_t used_count; //!< Количество используемых каналов.
    bool enabled; //!< Разрешение каналов.
    decim_t decim; //!< Дециматор.
    size_t samples; //!< Количество семплов.
    size_t index; //!< Текущий индекс.
    //size_t rate; //!< Коэффициент деления частоты дискретизации.
    iq15_t time; //!< Время осциллограммы.
    bool pause_enabled; //!< Разрешение паузы.
    size_t pause_counter; //!< Счётчик семплов до паузы.
    size_t pause_samples; //!< Число семплов до паузы.
    struct timeval end_time; //!< Время последнего семпла осциллограммы.
    osc_pool_t pool; //!< Пул данных.
} osc_t;

//! Осциллограммы.
static osc_t osc;

/*
 * Пул.
 */
/*
//! Инициализирует пул.
static void osc_pool_init(void)
{
    osc_pool_t* pool = &osc.pool;

    pool->index = 0;
    memset(pool->data, 0x0, OSC_SAMPLES_MAX * sizeof(osc_value_t));
}*/

//! Сбрасывает пул.
static void osc_pool_reset(void)
{
    osc_pool_t* pool = &osc.pool;

    pool->index = 0;
}

//! Выделяет данные из пула.
static osc_value_t* osc_pool_alloc(size_t count)
{
    osc_pool_t* pool = &osc.pool;

    if((OSC_TOTAL_SAMPLES_MAX - pool->index) < count) return NULL;

    osc_value_t* data = &pool->data[pool->index];

    pool->index += count;

    return data;
}

/*
 * Каналы.
 */
//! Получает канал по номеру.
ALWAYS_INLINE static osc_channel_t* osc_channel(size_t n)
{
    return &osc.channels[n];
}

void osc_init(void)
{
    memset(&osc, 0x0, sizeof(osc_t));
}

static size_t osc_index_next(size_t index)
{
	size_t next = index + 1;

	if(next >= osc.samples) next = 0;

	return next;
}

ALWAYS_INLINE static void osc_index_inc(void)
{
    osc.index = osc_index_next(osc.index);
}

/**
 * Помещает значение в массив данных канала.
 * @param channel Канал.
 * @param value Значение.
 */
static void osc_channel_put_value_val(osc_channel_t* channel, osc_value_t value)
{
	channel->data[osc.index] = value;
}

/**
 * Помещает битовое значение в данные канала.
 * @param channel Канал.
 * @param value Битовое значение.
 */
static void osc_channel_put_value_bit(osc_channel_t* channel, osc_value_t value)
{
	size_t pos = osc.index / OSC_BITS_PER_SAMPLE;
	size_t bit = osc.index % OSC_BITS_PER_SAMPLE;

	if(value){
		channel->data[pos] |= (1 << bit);
	}else{
		channel->data[pos] &= ~(1 << bit);
	}
}

/**
 * Помещает значение в канал по типу.
 * @param channel Канал.
 * @param value Значение.
 */
static void osc_channel_put_value(osc_channel_t* channel, osc_value_t value)
{
	if(channel->type == OSC_VAL){
        osc_channel_put_value_val(channel, value);
	}else{// OSC_BIT
        osc_channel_put_value_bit(channel, value);
	}
}

/**
 * Получает значение данных канала.
 * @param channel Канал.
 * @param index Индекс.
 * @return Значение.
 */
static osc_value_t osc_channel_get_value_val(osc_channel_t* channel, size_t index)
{
	if(index >= osc.samples) return 0;

	return channel->data[index];
}

/**
 * Получает битовое значение данных канала.
 * @param channel Канал.
 * @param index Индекс.
 * @return Значение.
 */
static osc_value_t osc_channel_get_value_bit(osc_channel_t* channel, size_t index)
{
	size_t pos = index / OSC_BITS_PER_SAMPLE;
	size_t bit = index % OSC_BITS_PER_SAMPLE;

	if(index >= osc.samples) return 0;

	return (channel->data[pos] & (1 << bit)) ? 1 : 0;
}

/**
 * Получает значение данных канала по типу.
 * @param channel Канал.
 * @param index Индекс.
 * @return Значение.
 */
static osc_value_t osc_channel_get_value(osc_channel_t* channel, size_t index)
{
	if(channel->type == OSC_VAL){
		return osc_channel_get_value_val(channel, index);
	}
	return osc_channel_get_value_bit(channel, index);
}

/**
 * Добавляет значение в усреднение канала.
 * @param channel Канал.
 * @param value Значение.
 */
ALWAYS_INLINE static void osc_channel_append_value(osc_channel_t* channel, osc_value_t value)
{
    if(channel->type == OSC_VAL){
        avg_put(&channel->avg, value);
    }else{
        maj_put(&channel->maj, value);
    }
}

/**
 * Добавляет значение аналогового входа.
 * @param channel Канал.
 */
static void osc_channel_append_ain(osc_channel_t* channel)
{
	q15_t q = 0;
	osc_value_t value = 0;

	if(channel->src_type == OSC_INST){
		q = ain_value_inst(channel->src_channel);
	}else{
		q = ain_value(channel->src_channel);
	}

	value = q;

	osc_channel_append_value(channel, value);
}

/**
 * Добавляет значение цифрового входа.
 * @param channel Канал.
 */
static void osc_channel_append_din(osc_channel_t* channel)
{
	osc_value_t value = 0;

	value = din_state(channel->src_channel);

	osc_channel_append_value(channel, value);
}

/**
 * Добавляет данные в канал.
 * @param channel Канал.
 * @return Флаг добавления.
 */
static bool osc_channel_append(osc_channel_t* channel)
{
	if(!channel->enabled) return false;
	if(channel->size == 0) return false;
	if(channel->data == NULL) return false;

	switch(channel->src){
	case OSC_AIN:
		osc_channel_append_ain(channel);
		break;
	case OSC_DIN:
		osc_channel_append_din(channel);
		break;
	default:
		return false;
	}

	return true;
}

/**
 * Помещает данные в канал.
 * @param channel Канал.
 * @return Флаг добавления.
 */
static bool osc_channel_put(osc_channel_t* channel)
{
    if(!channel->enabled) return false;
    if(channel->size == 0) return false;
    if(channel->data == NULL) return false;

    osc_value_t value = 0;

    if(channel->type == OSC_VAL){
        value = avg_calc(&channel->avg);
    }else{
        value = maj_calc(&channel->maj);
    }

    osc_channel_put_value(channel, value);

    return true;
}

void osc_append(void)
{
	if(!osc.enabled) return;

	// Если разрешена пауза.
	if(osc.pause_enabled){
	    // Если достигли окончания интервала ожидания.
	    if(osc.pause_counter == osc.pause_samples){
	        // Если время останова нулевое (не зафиксировано).
	        if(!timerisset(&osc.end_time)){
	            // Получим текущее время.
	            gettimeofday(&osc.end_time, NULL);
	            // Время между семплами.
	            struct timeval sample_tv;
	            osc_sample_period(&sample_tv);
	            // Вычтем время с последнего семпла.
	            timersub(&osc.end_time, &sample_tv, &osc.end_time);
	        }
	        // Возврат.
	        return;
	    }
	    osc.pause_counter ++;
	}

	osc_channel_t* channel = NULL;
	size_t n = 0;
	size_t i;

	// Добавим данные в усреднение каналов.
	for(i = 0; i < osc.used_count; i ++){
		n = osc.used_map[i];

		channel = osc_channel(n);

		osc_channel_append(channel);
	}

	// Увеличим индекс дециматора.
	decim_put(&osc.decim, 0);

	// Если пора записывать осциллограмму -
	// поместим данные в буферы каналов.
	if(decim_ready(&osc.decim)){
	    // Добавим данные в усреднение каналов.
	    for(i = 0; i < osc.used_count; i ++){
	        n = osc.used_map[i];

	        channel = osc_channel(n);

	        // Поместим значение в буфер данных.
	        osc_channel_put(channel);
	    }

	    // Увеличим индекс.
	    osc_index_inc();
	}
}

void osc_set_enabled(bool enabled)
{
	osc.enabled = enabled;
}

bool osc_enabled(void)
{
	return osc.enabled;
}

void osc_reset(void)
{
	size_t n = 0;

	size_t i;
	for(i = 0; i < osc.used_count; i ++){
		n = osc.used_map[i];

        osc_channel_reset(n);
	}

	// Дециматор.
	decim_reset(&osc.decim);

	// Семплы и индекс.
	osc.samples = 0;
	osc.index = 0;

    // Память будет освобождена, т.к.
	// все используемые каналы сброшены,
    // буферы каналов больше не действительны.
    osc.used_count = 0;

    // Сбросим пул данных.
    osc_pool_reset();

    // Время.
    osc.time = 0;

    // Пауза.
    osc.pause_enabled = false;
    osc.pause_samples = 0;
}

size_t osc_used_channels(void)
{
	return osc.used_count;
}

iq15_t osc_time(void)
{
    return osc.time;
}

size_t osc_rate(void)
{
    //return osc.rate;
    return decim_scale(&osc.decim);
}

size_t osc_samples(void)
{
    return osc.samples;
}

size_t osc_index(void)
{
    return osc.index;
}

size_t osc_next_index(size_t index)
{
    return osc_index_next(index);
}

size_t osc_sample_index(size_t sample)
{
    size_t index = sample + osc.index;

    if (index >= osc.samples) index -= osc.samples;

    return index;
}

size_t osc_skew(void)
{
    return decim_skew(&osc.decim);
}

void osc_pause(iq15_t time)
{
    lq15_t time_samples = iq15_imull(time, AIN_SAMPLE_FREQ);
    size_t samples = (size_t)IQ15_INT(time_samples);

    timerclear(&osc.end_time);
    osc.pause_counter = 0;
    osc.pause_samples = samples;
    osc.pause_enabled = true;
}

bool osc_paused(void)
{
    return osc.pause_enabled && (osc.pause_counter == osc.pause_samples);
}

void osc_resume(void)
{
    osc.pause_enabled = false;
}

void osc_end_time(struct timeval* tv)
{
    if(!tv) return;

    tv->tv_sec = osc.end_time.tv_sec;
    tv->tv_usec = osc.end_time.tv_usec;
}

void osc_start_time(struct timeval* tv)
{
    if(!tv) return;

    struct timeval time_tv;
    osc_end_time(&time_tv);

    struct timeval period_tv;
    osc_sample_period(&period_tv);

    size_t i;
    for(i = 0; i < osc.samples; i ++){
        timersub(&time_tv, &period_tv, &time_tv);
    }

    tv->tv_sec = time_tv.tv_sec;
    tv->tv_usec = time_tv.tv_usec;
}

void osc_sample_period(struct timeval* tv)
{
    if(!tv) return;

    tv->tv_sec = 0;
    tv->tv_usec = AIN_SAMPLE_PERIOD_US * osc_rate();
}

err_t osc_channel_set_enabled(size_t n, bool enabled)
{
	if(n >= AIN_CHANNELS_COUNT) return E_OUT_OF_RANGE;

	osc_channel_t* channel = osc_channel(n);

	channel->enabled = enabled;

    return E_NO_ERROR;
}

bool osc_channel_enabled(size_t n)
{
	if(n >= AIN_CHANNELS_COUNT) return false;

	osc_channel_t* channel = osc_channel(n);

	return channel->enabled;
}

err_t osc_channel_reset(size_t n)
{
	if(n >= AIN_CHANNELS_COUNT) return false;

	osc_channel_t* channel = osc_channel(n);

	channel->enabled = false;
	channel->size = 0;
	channel->data = NULL;

	if(channel->type == OSC_VAL){
	    avg_reset(&channel->avg);
	}else{
	    maj_reset(&channel->maj);
	}

	return E_NO_ERROR;
}

osc_src_t osc_channel_src(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    osc_channel_t* channel = osc_channel(n);

    return channel->src;
}

osc_type_t osc_channel_type(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    osc_channel_t* channel = osc_channel(n);

    return channel->type;
}

osc_src_type_t osc_channel_src_type(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    osc_channel_t* channel = osc_channel(n);

    return channel->src_type;
}

size_t osc_channel_src_channel(size_t n)
{
    if(n >= AIN_CHANNELS_COUNT) return 0;

    osc_channel_t* channel = osc_channel(n);

    return channel->src_channel;
}

osc_value_t osc_channel_value(size_t n, size_t index)
{
	if(n >= AIN_CHANNELS_COUNT) return 0;

	osc_channel_t* channel = osc_channel(n);

	return osc_channel_get_value(channel, index);
}

err_t osc_channel_init(size_t n, osc_src_t src, osc_type_t type, osc_src_type_t src_type, size_t src_channel)
{
	if(n >= AIN_CHANNELS_COUNT) return false;

	osc_channel_t* channel = osc_channel(n);

	osc_channel_reset(n);

	channel->src = src;
	channel->type = type;
	channel->src_type = src_type;
	channel->src_channel = src_channel;

	return E_NO_ERROR;
}

//! Вычисляет необходимый размер буфера в зависимости от типа данных канала.
static size_t osc_channel_count_to_size(osc_channel_t* channel, size_t count)
{
	size_t size = 0;

	// Для битовых значений.
	if(channel->type == OSC_BIT){
		// Учтём побитовую упаковку.
		size = (count + OSC_BITS_PER_SAMPLE - 1) / OSC_BITS_PER_SAMPLE;
	}else{ // Для аналоговых значений.
		size = count;
	}

	return size;
}

/**
 * Вычисляет размер данных группы каналов,
 * эквивалентный по времени записи максимальной длине буфера.
 * Устанавливает значение размера в канале.
 * Игнорирует запрещённые каналы и
 * не настроенные каналы.
 * @param channels Каналы.
 * @param count Число каналов.
 * @return Размер.
 */
static size_t osc_channels_calc_req_size(osc_channel_t* channels, size_t count)
{
    osc_channel_t* channel = NULL;

    size_t req_size = 0;

    size_t i;
    for(i = 0; i < count; i ++){
    	channel = osc_channel(i);

    	// Пропуск запрещённых каналов.
    	if(!channel->enabled) continue;

    	channel->size = osc_channel_count_to_size(channel, OSC_SAMPLES_MAX);

        req_size += channel->size;
    }

    return req_size;
}

/**
 * Вычисляет необходимое число семплов для каждого канала.
 * @param rate Коэффициент числа семплов.
 * @return Необходимое число семплов.
 */
static size_t osc_calc_channel_samples(iq15_t rate)
{
    // Необходимый размер.
    int32_t samples_count = OSC_SAMPLES_MAX;
    // Скорректируем.
    samples_count = iq15_imul(rate, samples_count);
    // Округлим до целого в меньшую сторону.
    samples_count = iq15_int(samples_count);

    return samples_count;
}

/**
 * Выделяет буферы данных каналов.
 * Игнорирует запрещённые каналы и
 * каналы с нулевым числов семплов.
 * @param channels Каналы.
 * @param count Число каналов.
 * @param rate Относительный размер.
 * @return Код ошибки.
 */
static err_t osc_alloc_buffers(osc_channel_t* channels, size_t count, size_t samples_count)
{
    osc_channel_t* channel = NULL;
    osc_value_t* data = NULL;
    int32_t size = 0;

    size_t i;
    for(i = 0; i < count; i ++){
        channel = &channels[i];

    	// Пропуск запрещённых каналов.
    	if(!channel->enabled) continue;

    	// Вычислим размер буфера.
    	size = osc_channel_count_to_size(channel, samples_count);

    	// Минимальный размер.
		if(size == 0) size = OSC_BUF_SIZE_MIN;

        // Выделим буфер данных.
        data = osc_pool_alloc(size);
        // Недостаточно памяти.
        if(data == NULL) return E_OUT_OF_MEMORY;

        channel->data = data;
        channel->size = size;
    }

    return E_NO_ERROR;
}

/**
 * Формирует карту используемых каналов.
 */
static void osc_make_used_map(void)
{
	size_t n = 0;
	osc_channel_t* channel = NULL;

	size_t i;
	for(i = 0; i < OSC_COUNT_MAX; i ++){
		channel = osc_channel(i);

		if(channel->enabled){
			osc.used_map[n] = i;
			n ++;
		}
	}
	osc.used_count = n;
}

/**
 * Вычисляет время записи осциллограмм.
 * @param osc_rate Коэффициент деления частоты дискретизации.
 * @param size_rate Относительный размер данных.
 * @return Время записи осциллограммы.
 */
static iq15_t osc_calc_time(size_t osc_rate, iq15_t size_rate)
{
    // Время буфера для осциллограмм.
    iq15_t buf_time = (iq15_t)LQ15F(OSC_SAMPLES_MAX, AIN_SAMPLE_FREQ);
    // Учтём снижение частоты дискретизации.
    buf_time = iq15_imul(buf_time, osc_rate);
    // Вычислим время.
    iq15_t time = iq15_mull(buf_time, size_rate);

    return time;
}

err_t osc_init_channels(size_t rate)
{
    if(rate == 0) return E_INVALID_VALUE;

    err_t err = E_NO_ERROR;

    size_t total_req_size = osc_channels_calc_req_size(osc.channels, OSC_COUNT_MAX);
    size_t total_size = OSC_SAMPLES_MAX;

    // Вычислим относительный размер для всех каналов.
    //iq15_t size_rate = IQ15F(total_size, total_req_size);
    // Для поддержки буферов большого объёма (больше 64кб).
    iq15_t size_rate = (iq15_t)((((int64_t)total_size) << Q15_FRACT_BITS) / total_req_size);

    // Вычислим число семплов.
    size_t samples_count = osc_calc_channel_samples(size_rate);
    // Если число выходит за пределы - ошибка.
    if(samples_count < OSC_SAMPLES_MIN || samples_count > OSC_SAMPLES_MAX) return E_OUT_OF_RANGE;

    // Выделим память.
    err = osc_alloc_buffers(osc.channels, OSC_COUNT_MAX, samples_count);
    if(err != E_NO_ERROR) return err;

    // Используемые каналы.
    osc_make_used_map();

    // Дециматоры.
    decim_init(&osc.decim, rate);

    osc.index = 0;
    osc.samples = samples_count;
    //osc.rate = rate;
    osc.time = osc_calc_time(rate, size_rate);

    return E_NO_ERROR;
}
