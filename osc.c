#include "osc.h"
#include "decim.h"
#include "decim_avg.h"
#include "ain.h"
#include "din.h"


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


//! Структура канала осциллограммы.
typedef struct _Osc_Channel {
    osc_src_t src; //!< Источник данных.
    osc_type_t type; //!< Тип значения.
    osc_src_type_t src_type; //!< Тип - мгновенное или действующее.
    size_t src_channel; //!< Номер канала источника данных.
    union {
    	decim_avg_t decim_avg; //!< Усредняющий дециматор.
    	decim_t decim_inst; //!< Прореживающий дециматор.
    }; //!< Дециматоры.
    osc_value_t* data; //!< Данные.
    size_t size; //!< Размер данных в единицах буфера.
    size_t count; //!< Размер данных в семплах канала.
    size_t index; //!< Текущий индекс.
    size_t skew; //!< Смещение в семплах начала осциллограммы.
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
    size_t used_channels; //!< Количество используемых каналов.
    bool enabled; //!< Разрешение каналов.
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

//! Получает степень децимации.
ALWAYS_INLINE static size_t osc_channel_decim_scale(osc_channel_t* channel)
{
	if(channel->type == OSC_VAL) return decim_avg_scale(&channel->decim_avg);
	return decim_scale(&channel->decim_inst);
}

//! Инициализирует дециматор.
ALWAYS_INLINE static err_t osc_channel_decim_init(osc_channel_t* channel, size_t scale)
{
	if(channel->type == OSC_VAL) return decim_avg_init(&channel->decim_avg, scale);
	return decim_init(&channel->decim_inst, scale);
}

//! Сбрасывает дециматор.
ALWAYS_INLINE static void osc_channel_decim_reset(osc_channel_t* channel)
{
	if(channel->type == OSC_VAL) decim_avg_reset(&channel->decim_avg);
	decim_reset(&channel->decim_inst);
}

void osc_init(void)
{
    memset(&osc, 0x0, sizeof(osc_t));
}

static size_t osc_channel_index_next(osc_channel_t* channel, size_t index)
{
	size_t next = index + 1;

	//size_t last = channel->size;
	//if(channel->type == OSC_BIT) last *= OSC_BITS_PER_SAMPLE;

	if(next >= channel->count) next = 0;

	return next;
}

/**
 * Добавляет значение в массив данных канала.
 * @param channel Канал.
 * @param value Значение.
 */
static void osc_channel_append_value_val(osc_channel_t* channel, osc_value_t value)
{
	channel->data[channel->index] = value;
	channel->index = osc_channel_index_next(channel, channel->index);
}

/**
 * Добавляет битовое значение в данные канала.
 * @param channel Канал.
 * @param value Битовое значение.
 */
static void osc_channel_append_value_bit(osc_channel_t* channel, osc_value_t value)
{
	size_t pos = channel->index / OSC_BITS_PER_SAMPLE;
	size_t bit = channel->index % OSC_BITS_PER_SAMPLE;

	if(value){
		channel->data[pos] |= (1 << bit);
	}else{
		channel->data[pos] &= ~(1 << bit);
	}

	channel->index = osc_channel_index_next(channel, channel->index);
}

/**
 * Добавляет значение в канал по типу.
 * @param channel Канал.
 * @param value Значение.
 */
static void osc_channel_append_value(osc_channel_t* channel, osc_value_t value)
{
	if(channel->type == OSC_VAL){

		decim_avg_put(&channel->decim_avg, value);

		if(decim_avg_ready(&channel->decim_avg)){
			channel->skew = 0;

			value = decim_avg_data(&channel->decim_avg);

			osc_channel_append_value_val(channel, value);
		}else{
			channel->skew ++;
		}
	}else{// OSC_BIT

		decim_put(&channel->decim_inst, value);

		if(decim_ready(&channel->decim_inst)){
			channel->skew = 0;

			value = decim_data(&channel->decim_inst);

			osc_channel_append_value_bit(channel, value);
		}else{
			channel->skew ++;
		}
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
	if(index >= channel->count) return 0;

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

	if(index >= channel->count) return 0;

	return (channel->data[pos] &= (1 << bit)) ? 1 : 0;
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

	// convert from q15.
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

	// TMP.
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
	if(channel->count == 0) return false;
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

void osc_append(void)
{
	if(!osc.enabled) return;

	osc_channel_t* channel = NULL;

	size_t i;
	for(i = 0; i < osc.used_channels; i ++){
		channel = osc_channel(i);

		osc_channel_append(channel);
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

/**
 * Сбрасывает каналы.
 * @param channels Каналы.
 * @param count Число каналов.
 */
static void osc_channels_reset(osc_channel_t* channels, size_t count)
{
    size_t i;
    for(i = 0; i < count; i++){
        osc_channel_reset(i);
    }
}

void osc_reset(void)
{
	osc_channels_reset(osc.channels, osc.used_channels);
}

size_t osc_used_channels(void)
{
	return osc.used_channels;
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

	osc_channel_decim_reset(channel);

	channel->skew = 0;
	channel->index = 0;

	return E_NO_ERROR;
}

size_t osc_channel_samples(size_t n)
{
	if(n >= AIN_CHANNELS_COUNT) return 0;

	osc_channel_t* channel = osc_channel(n);

	return channel->count;
}

size_t osc_channel_index(size_t n)
{
	if(n >= AIN_CHANNELS_COUNT) return 0;

	osc_channel_t* channel = osc_channel(n);

	return channel->index;
}

size_t osc_channel_next_index(size_t n, size_t index)
{
	if(n >= AIN_CHANNELS_COUNT) return 0;

	osc_channel_t* channel = osc_channel(n);

	return osc_channel_index_next(channel, index);
}

osc_value_t osc_channel_value(size_t n, size_t index)
{
	if(n >= AIN_CHANNELS_COUNT) return 0;

	osc_channel_t* channel = osc_channel(n);

	return osc_channel_get_value(channel, index);
}

err_t osc_channel_init(size_t n, osc_src_t src, osc_type_t type, osc_src_type_t src_type, size_t src_channel, size_t rate)
{
	if(n >= AIN_CHANNELS_COUNT) return false;

	osc_channel_t* channel = osc_channel(n);

	channel->enabled = false;
	channel->size = 0;
	channel->count = 0;
	channel->index = 0;
	channel->data = NULL;

	channel->src = src;
	channel->type = type;
	channel->src_type = src_type;
	channel->src_channel = src_channel;

	osc_channel_decim_init(channel, rate);
	channel->skew = 0;

	return E_NO_ERROR;
}

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
 * @param channels Каналы.
 * @param count Число каналов.
 * @return Размер.
 */
static size_t osc_channels_calc_req_size(osc_channel_t* channels, size_t count)
{
    osc_channel_t* channel = NULL;
    size_t samples = OSC_SAMPLES_MAX;
    size_t rate = 0;

    size_t rel_size = 0;

    size_t i;
    for(i = 0; i < count; i ++){
    	channel = osc_channel(i);

    	rate = osc_channel_decim_scale(channel);
    	if(rate) channel->count = samples / rate;

    	// Минимальное число семплов.
    	if(channel->count == 0) channel->count = OSC_SAMPLES_MIN;

    	channel->size = osc_channel_count_to_size(channel, channel->count);

    	// Минимальный размер.
		if(channel->size == 0) channel->size = OSC_BUF_SIZE_MIN;

        rel_size += channel->size;
    }

    return rel_size;
}

/**
 * Выделяет буферы данных каналов.
 * @param channels Каналы.
 * @param count Число каналов.
 * @param rate Относительный размер.
 * @return Код ошибки.
 */
static err_t osc_alloc_buffers(osc_channel_t* channels, size_t count, iq15_t rate)
{
    osc_channel_t* channel = NULL;
    osc_value_t* data = NULL;
    int32_t samples_count = 0;
    int32_t size = 0;

    size_t i;
    for(i = 0; i < count; i ++){
        channel = &channels[i];

        // Необходимый размер.
        samples_count = channel->count;
        // Скорректируем.
        samples_count = iq15_imul(rate, samples_count);
        // Округлим до целого в меньшую сторону.
        samples_count = iq15_int(samples_count);

    	// Минимальное число семплов.
    	if(samples_count == 0) samples_count = OSC_SAMPLES_MIN;

    	size = osc_channel_count_to_size(channel, samples_count);

    	// Минимальный размер.
		if(size == 0) size = OSC_BUF_SIZE_MIN;

        // Выделим буфер данных.
        data = osc_pool_alloc(size);
        // Недостаточно памяти.
        if(data == NULL) return E_OUT_OF_MEMORY;

        channel->data = data;
        channel->count = samples_count;
        channel->size = size;
    }

    return E_NO_ERROR;
}

err_t osc_init_channels(size_t count)
{
    if(count == 0) return E_INVALID_VALUE;
    if(count >= OSC_COUNT_MAX) return E_OUT_OF_RANGE;

    err_t err = E_NO_ERROR;

    // Память будет освобождена,
    // буферы каналов больше не действительны.
    osc.used_channels = 0;

    // Сбросим пул данных.
    osc_pool_reset();

    size_t total_req_size = osc_channels_calc_req_size(osc.channels, count);
    size_t total_size = OSC_SAMPLES_MAX;

    // Вычислим относительный размер для всех каналов.
    //iq15_t size_rate = IQ15F(total_size, total_req_size);
    // Для поддержки буферов большого объёма (больше 64кб).
    iq15_t size_rate = (iq15_t)((((int64_t)total_size) << Q15_FRACT_BITS) / total_req_size);

    // Выделим память.
    err = osc_alloc_buffers(osc.channels, count, size_rate);
    if(err != E_NO_ERROR) return err;

    // Используемые каналы.
    osc.used_channels = count;

    return E_NO_ERROR;
}
