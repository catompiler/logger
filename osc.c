#include "osc.h"
#include "ain.h"
#include "din.h"
#include <string.h>


/*
 * Пул.
 */

//! Инициализирует пул.
static void osc_pool_init(osc_pool_t* pool, osc_value_t* data, size_t size)
{
    pool->data = data;
    pool->size = size;
    pool->index = 0;
}
/*
//! Получает размер данных пула.
ALWAYS_INLINE static size_t osc_pool_size(osc_pool_t* pool)
{
    return pool->size;
}

//! Сбрасывает пул.
ALWAYS_INLINE static void osc_pool_reset(osc_pool_t* pool)
{
    pool->index = 0;
}
*/
//! Выделяет данные из пула.
static size_t osc_pool_alloc_index(osc_pool_t* pool, size_t count)
{
    if((pool->size - pool->index) < count) return OSC_INDEX_INVALID;

    size_t index = pool->index;

    pool->index += count;

    return index;
}

//! Выделяет данные из пула.
static osc_value_t* osc_pool_alloc(osc_pool_t* pool, size_t count)
{
    size_t index = osc_pool_alloc_index(pool, count);

    if(index == OSC_INDEX_INVALID) return NULL;

    return &pool->data[index];
}

/*
 * Общее.
 */
//! Получает следующий индекс семплов.
static size_t osc_sample_index_next(osc_t* osc, size_t index)
{
    size_t next = index + 1;

    if(next >= osc->samples) next = 0;

    return next;
}

/*
 * Буфера.
 */
//! Получает буфер по номеру.
ALWAYS_INLINE static osc_buffer_t* osc_buffer(osc_t* osc, size_t n)
{
    return &osc->buffers[n];
}

//! Получает буфер записи.
ALWAYS_INLINE static osc_buffer_t* osc_put_buffer(osc_t* osc)
{
    return osc_buffer(osc, osc->put_buf_index);
}
/*
//! Получает буфер чтения.
ALWAYS_INLINE static osc_buffer_t* osc_get_buffer(osc_t* osc)
{
    return osc_buffer(osc, osc->get_buf_index);
}
*/
//! Получает следующий индекс буфера.
static size_t osc_buffer_index_next(osc_t* osc, size_t index)
{
    size_t next = index + 1;

    if(next >= osc->buffers_count) next = 0;

    return next;
}

//! Инициализирует буфер.
static void osc_buffer_init(osc_buffer_t* buffer, osc_value_t* data, size_t size)
{
    buffer->data = data;
    buffer->size = size;
    buffer->index = 0;
    buffer->count = 0;
    buffer->end_time.tv_sec = 0;
    buffer->end_time.tv_usec = 0;
    buffer->paused = false;
}

//! Сбрасывает буфер.
static void osc_buffer_reset(osc_buffer_t* buffer)
{
    buffer->index = 0;
    buffer->count = 0;
    buffer->end_time.tv_sec = 0;
    buffer->end_time.tv_usec = 0;
    buffer->paused = false;
}

//! Инкрементирует индекс семплов буфера.
ALWAYS_INLINE static void osc_sample_index_inc(osc_t* osc, osc_buffer_t* buffer)
{
    buffer->index = osc_sample_index_next(osc, buffer->index);
}

//! Инкрементирует число семплов в буфере.
ALWAYS_INLINE static void osc_buffer_count_inc(osc_t* osc, osc_buffer_t* buffer)
{
    size_t inc_count = buffer->count + 1;
    if(buffer->count < osc->samples) buffer->count = inc_count;
}

//! Устанавливает индекс следующего буфера чтения.
ALWAYS_INLINE static void osc_buffers_inc_get_index(osc_t* osc)
{
    osc->get_buf_index = osc_buffer_index_next(osc, osc->get_buf_index);
}

//! Устанавливает индекс следующего буфера записи.
ALWAYS_INLINE static void osc_buffers_inc_put_index(osc_t* osc)
{
    osc->put_buf_index = osc_buffer_index_next(osc, osc->put_buf_index);
}

//! Получает данные канала в буфере.
ALWAYS_INLINE static osc_value_t* osc_channel_buffer_data(osc_channel_t* channel, osc_buffer_t* buffer)
{
    return &buffer->data[channel->offset];
}


//! Инициализирует буферы осциллограмм.
static err_t osc_init_buffers(osc_t* osc)
{
    osc_buffer_t* buffer = NULL;
    osc_value_t* data = NULL;
    size_t buf_size = osc->data_size / osc->buffers_count;

    osc_pool_t buf_pool;
    osc_pool_init(&buf_pool, osc->data, osc->data_size);

    size_t i;
    for(i = 0; i < osc->buffers_count; i ++){
        data = osc_pool_alloc(&buf_pool, buf_size);
        if(data == NULL) return E_OUT_OF_MEMORY;

        buffer = osc_buffer(osc, i);
        osc_buffer_init(buffer, data, buf_size);
    }

    osc->buf_samples = buf_size;
    osc->put_buf_index = 0;
    osc->get_buf_index = 0;

    return E_NO_ERROR;
}
/*
 * Каналы.
 */
//! Получает канал по номеру.
ALWAYS_INLINE static osc_channel_t* osc_channel(osc_t* osc, size_t n)
{
    return &osc->channels[n];
}

err_t osc_init(osc_t* osc, osc_value_t* data, size_t data_size,
               osc_buffer_t* buffers, size_t buffers_count,
               osc_channel_t* channels, size_t channels_count)
{
    if(data == NULL) return E_NULL_POINTER;
    if(data_size == 0) return E_INVALID_VALUE;
    if(buffers == NULL) return E_NULL_POINTER;
    if(buffers_count == 0) return E_INVALID_VALUE;
    if(channels == NULL) return E_NULL_POINTER;
    if(channels_count == 0) return E_INVALID_VALUE;

    err_t err = E_NO_ERROR;

    memset(osc, 0x0, sizeof(osc_t));

    osc->data = data;
    osc->data_size = data_size;
    osc->buffers = buffers;
    osc->buffers_count = buffers_count;
    osc->channels = channels;
    osc->channels_count = channels_count;

    osc->buffer_mode = OSC_RING_IN_BUFFER;

    err = osc_init_buffers(osc);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

osc_buffer_mode_t osc_buffer_mode(osc_t* osc)
{
    return osc->buffer_mode;
}

void osc_set_buffer_mode(osc_t* osc, osc_buffer_mode_t mode)
{
    osc->buffer_mode = mode;
}

/**
 * Помещает значение в массив данных канала.
 * @param channel Канал.
 * @param value Значение.
 */
static void osc_channel_put_value_val(osc_channel_t* channel, osc_buffer_t* buffer, osc_value_t value)
{
    osc_value_t* data = osc_channel_buffer_data(channel, buffer);

	data[buffer->index] = value;
}

/**
 * Помещает битовое значение в данные канала.
 * @param channel Канал.
 * @param value Битовое значение.
 */
static void osc_channel_put_value_bit(osc_channel_t* channel, osc_buffer_t* buffer, osc_value_t value)
{
	size_t pos = buffer->index / OSC_BITS_PER_SAMPLE;
	size_t bit = buffer->index % OSC_BITS_PER_SAMPLE;

	osc_value_t* data = osc_channel_buffer_data(channel, buffer);

	if(value){
		data[pos] |= (1 << bit);
	}else{
		data[pos] &= ~(1 << bit);
	}
}

/**
 * Помещает значение в канал по типу.
 * @param channel Канал.
 * @param value Значение.
 */
static void osc_channel_put_value(osc_channel_t* channel, osc_buffer_t* buffer, osc_value_t value)
{
	if(channel->type == OSC_VAL){
        osc_channel_put_value_val(channel, buffer, value);
	}else{// OSC_BIT
        osc_channel_put_value_bit(channel, buffer, value);
	}
}

/**
 * Получает значение данных канала.
 * @param channel Канал.
 * @param index Индекс.
 * @return Значение.
 */
static osc_value_t osc_channel_get_value_val(osc_t* osc, osc_channel_t* channel, osc_buffer_t* buffer, size_t index)
{
	if(index >= buffer->count) return 0;

	osc_value_t* data = osc_channel_buffer_data(channel, buffer);

	return data[index];
}

/**
 * Получает битовое значение данных канала.
 * @param channel Канал.
 * @param index Индекс.
 * @return Значение.
 */
static osc_value_t osc_channel_get_value_bit(osc_t* osc, osc_channel_t* channel, osc_buffer_t* buffer,  size_t index)
{
    if(index >= buffer->count) return 0;

	size_t pos = index / OSC_BITS_PER_SAMPLE;
	size_t bit = index % OSC_BITS_PER_SAMPLE;

	osc_value_t* data = osc_channel_buffer_data(channel, buffer);

	return (data[pos] & (1 << bit)) ? 1 : 0;
}

/**
 * Получает значение данных канала по типу.
 * @param channel Канал.
 * @param index Индекс.
 * @return Значение.
 */
static osc_value_t osc_channel_get_value(osc_t* osc, osc_channel_t* channel, osc_buffer_t* buffer, size_t index)
{
	if(channel->type == OSC_VAL){
		return osc_channel_get_value_val(osc, channel, buffer, index);
	}
	return osc_channel_get_value_bit(osc, channel, buffer, index);
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

	if(channel->src_type == OSC_INST){
	    value = din_state_inst(channel->src_channel);
    }else{
        value = din_state(channel->src_channel);
    }

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
	if(channel->offset == OSC_INDEX_INVALID) return false;

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
static bool osc_channel_put(osc_channel_t* channel, osc_buffer_t* buffer)
{
    if(!channel->enabled) return false;
    if(channel->offset == OSC_INDEX_INVALID) return false;

    osc_value_t value = 0;

    if(channel->type == OSC_VAL){
        value = avg_calc(&channel->avg);
    }else{
        value = maj_calc(&channel->maj);
    }

    osc_channel_put_value(channel, buffer, value);

    return true;
}

//! Получает время последнего семпла.
static void osc_calc_last_sample_time(size_t skew, struct timeval* tv)
{
    if(!tv) return;
    // Время между семплами.
    struct timeval sample_tv = {0, AIN_SAMPLE_PERIOD_US};
    // Время последнего семпла.
    struct timeval end_time = {0, 0};

    // Получим текущее время.
    gettimeofday(&end_time, NULL);

    size_t i;
    for(i = 0; i < skew; i ++) {
        // Вычтем время с последнего семпла.
        timersub(&end_time, &sample_tv, &end_time);
    }

    tv->tv_sec = end_time.tv_sec;
    tv->tv_usec = end_time.tv_usec;
}

ALWAYS_INLINE static void osc_pause_mark_put_buffer(osc_t* osc, osc_buffer_t* buffer)
{
    // Обозначим паузу.
    buffer->paused = true;
}

ALWAYS_INLINE static void osc_pause_calc_sample_time(osc_t* osc, osc_buffer_t* buffer, size_t pause_skew)
{
    // Получим время паузы.
    osc_calc_last_sample_time(decim_skew(&osc->decim) + pause_skew, &buffer->end_time);
}

static void osc_pause_next_put_buffer(osc_t* osc)
{
    // Если необходимо - переключим буфер записи.
    switch(osc->buffer_mode){
    default:
        break;
    case OSC_BUFFER_IN_RING:
        break;
    case OSC_RING_IN_BUFFER:
        osc_buffers_inc_put_index(osc);
        break;
    }
}

static void osc_pause_put_buffer(osc_t* osc, osc_buffer_t* buffer, size_t pause_skew)
{
    // Остановим запись в текущий буфер.
    osc_pause_mark_put_buffer(osc, buffer);
    // Вычислим время последнего записанного семпла.
    osc_pause_calc_sample_time(osc, buffer, pause_skew);
}

static void osc_inc_put_index(osc_t* osc, osc_buffer_t* buffer)
{
    osc_sample_index_inc(osc, buffer);
    osc_buffer_count_inc(osc, buffer);
    //gettimeofday(&buffer->end_time, NULL);
}

static void osc_pause_full_buffer(osc_t* osc)
{
    osc_buffer_t* buffer = osc_put_buffer(osc);

    // Запись в буфер остановлена - возврат.
    if(buffer->paused) return;

    switch(osc->buffer_mode){
    default:
        break;
    case OSC_BUFFER_IN_RING:
        // Если число записанных семплов в буфер достигло предела.
        if(buffer->count >= osc->samples){
            // Остановим запись в буфер.
            osc_pause_put_buffer(osc, buffer, 1);
            // Перейдём на следующий буфер.
            osc_buffers_inc_put_index(osc);
        }
        break;
    case OSC_RING_IN_BUFFER:
        break;
    }
}

static void osc_pause_next_buffer(osc_t* osc)
{
    size_t put_index = osc->put_buf_index;

    osc_buffer_t* put_buf = osc_buffer(osc, put_index);

    if(!put_buf->paused) return;

    size_t next_put_index = osc_buffer_index_next(osc, put_index);

    osc_buffer_t* next_put_buf = osc_buffer(osc, next_put_index);

    if(next_put_buf->paused) return;

    osc_buffers_inc_put_index(osc);
}

void osc_append(osc_t* osc)
{
	if(!osc->enabled) return;

	// Буфер.
	osc_buffer_t* buffer = NULL;

	// Если разрешена пауза.
	if(osc->pause_enabled){
	    // Если достигли окончания интервала ожидания.
	    if(osc->pause_counter == osc->pause_samples){
	        // Запись в текущий буфер остановлена.
	        osc->pause_enabled = false;

	        // Получим буфер для останова записи.
	        buffer = osc_put_buffer(osc);
	        // Остановим запись в буфер.
	        osc_pause_put_buffer(osc, buffer, 1);
	        // Переключимся на следующий буфер, если это требуется.
	        osc_pause_next_put_buffer(osc);
	    }else{
	        osc->pause_counter ++;
	    }
	}

	// При необходимости - остановим и переключим заполненный буфер.
	osc_pause_full_buffer(osc);

	// При заполнении и паузе буфера - переключиться на следующий буфер.
	osc_pause_next_buffer(osc);

    // Текущий буфер вставки.
    buffer = osc_put_buffer(osc);

    // Если запись в текущий буфер остановлена - возврат.
    if(buffer->paused) return;

	osc_channel_t* channel = NULL;
	size_t i;

	// Добавим данные в усреднение каналов.
	for(i = 0; i < osc->channels_count; i ++){

		channel = osc_channel(osc, i);

		osc_channel_append(channel);
	}

	// Увеличим индекс дециматора.
	decim_put(&osc->decim, 0);

	// Если пора записывать осциллограмму -
	// поместим данные в буферы каналов.
	if(decim_ready(&osc->decim)){
	    // Добавим данные в усреднение каналов.
	    for(i = 0; i < osc->channels_count; i ++){

	        channel = osc_channel(osc, i);

	        // Поместим значение в буфер данных.
	        osc_channel_put(channel, buffer);
	    }

	    // Увеличим индекс.
	    osc_inc_put_index(osc, buffer);
	}
}

void osc_set_enabled(osc_t* osc, bool enabled)
{
	osc->enabled = enabled;
}

bool osc_enabled(osc_t* osc)
{
	return osc->enabled;
}

void osc_reset(osc_t* osc)
{
	size_t i;
	for(i = 0; i < osc->channels_count; i ++){
        osc_channel_reset(osc, i);
	}

	for(i = 0; i < osc->buffers_count; i ++){
	    osc_buffer_reset(osc_buffer(osc, i));
	}

	// Дециматор.
	decim_reset(&osc->decim);

	// Используемые каналы.
	osc->enabled_channels = 0;

	// Индексы буферов.
	osc->get_buf_index = 0;
	osc->put_buf_index = 0;

	// Количество семплов в буферах данных.
	osc->samples = 0;

    // Время.
    osc->time = 0;

    // Пауза.
    osc->pause_enabled = false;
    osc->pause_samples = 0;
    osc->pause_counter = 0;

    // Расчётные при инициализации данные.
    osc->enabled_channels = 0;
    osc->analog_channels = 0;
    osc->digital_channels = 0;
}

iq15_t osc_time(osc_t* osc)
{
    return osc->time;
}

size_t osc_rate(osc_t* osc)
{
    return decim_scale(&osc->decim);
}

size_t osc_channels_count(osc_t* osc)
{
    return osc->channels_count;
}

size_t osc_samples_count(osc_t* osc)
{
    return osc->samples;
}

size_t osc_buffers_count(osc_t* osc)
{
    return osc->buffers_count;
}

size_t osc_current_buffer(osc_t* osc)
{
    return osc->get_buf_index;
}

size_t osc_next_buffer(osc_t* osc)
{
    osc_buffers_inc_get_index(osc);

    return osc->get_buf_index;
}

size_t osc_next_buffer_index(osc_t* osc, size_t buf)
{
    return osc_buffer_index_next(osc, buf);
}

size_t osc_buffer_samples_count(osc_t* osc, size_t buf)
{
    if(buf >= osc->buffers_count) return 0;

    osc_buffer_t* buffer = osc_buffer(osc, buf);

    return buffer->count;
}

size_t osc_buffer_sample_index(osc_t* osc, size_t buf)
{
    if(buf >= osc->buffers_count) return 0;

    osc_buffer_t* buffer = osc_buffer(osc, buf);

    return buffer->index;
}

size_t osc_buffer_next_sample(osc_t* osc, size_t buf)
{
    if(buf >= osc->buffers_count) return 0;

    osc_buffer_t* buffer = osc_buffer(osc, buf);

    osc_sample_index_inc(osc, buffer);

    return buffer->index;
}

size_t osc_next_sample_index(osc_t* osc, size_t index)
{
    return osc_sample_index_next(osc, index);
}

size_t osc_buffer_sample_number_index(osc_t* osc, size_t buf, size_t sample)
{
    if(buf >= osc->buffers_count) return 0;

    osc_buffer_t* buffer = osc_buffer(osc, buf);

    if(sample >= buffer->count) return OSC_INDEX_INVALID;

    if(buffer->count < osc->samples) return sample;

    size_t index = sample + buffer->index;

    if (index >= buffer->count) index -= buffer->count;

    return index;
}

osc_value_t osc_buffer_channel_value(osc_t* osc, size_t buf, size_t n, size_t index)
{
    if(buf >= osc->buffers_count) return 0;
    if(n >= osc->channels_count) return 0;

    osc_buffer_t* buffer = osc_buffer(osc, buf);
    osc_channel_t* channel = osc_channel(osc, n);

    return osc_channel_get_value(osc, channel, buffer, index);
}

bool osc_buffer_paused(osc_t* osc, size_t buf)
{
    //return osc->pause_enabled && (osc->pause_counter == osc->pause_samples);
    if(buf >= osc->buffers_count) return false;

    osc_buffer_t* buffer = osc_buffer(osc, buf);

    return buffer->paused;
}

void osc_buffer_resume(osc_t* osc, size_t buf)
{
    //osc->pause_enabled = false;
    if(buf >= osc->buffers_count) return;

    osc_buffer_t* buffer = osc_buffer(osc, buf);

    osc_buffer_reset(buffer);
}

err_t osc_buffer_end_time(osc_t* osc, size_t buf, struct timeval* tv)
{
    if(buf >= osc->buffers_count) return E_OUT_OF_RANGE;
    if(!tv) return E_NULL_POINTER;

    osc_buffer_t* buffer = osc_buffer(osc, buf);

    tv->tv_sec = buffer->end_time.tv_sec;
    tv->tv_usec = buffer->end_time.tv_usec;

    return E_NO_ERROR;
}

err_t osc_buffer_start_time(osc_t* osc, size_t buf, struct timeval* tv)
{
    if(buf >= osc->buffers_count) return E_OUT_OF_RANGE;
    if(!tv) return E_NULL_POINTER;

    err_t err = E_NO_ERROR;

    struct timeval time_tv;
    err = osc_buffer_end_time(osc, buf, &time_tv);
    if(err != E_NO_ERROR) return err;

    struct timeval period_tv;
    err = osc_sample_period(osc, &period_tv);
    if(err != E_NO_ERROR) return err;

    size_t buf_samples = osc_buffer_samples_count(osc, buf);

    size_t i;
    for(i = 1; i < buf_samples; i ++){
        timersub(&time_tv, &period_tv, &time_tv);
    }

    tv->tv_sec = time_tv.tv_sec;
    tv->tv_usec = time_tv.tv_usec;

    return E_NO_ERROR;
}

err_t osc_buffer_sample_time(osc_t* osc, size_t buf, size_t sample, struct timeval* tv)
{
    if(buf >= osc->buffers_count) return E_OUT_OF_RANGE;
    if(!tv) return E_NULL_POINTER;

    err_t err = E_NO_ERROR;

    struct timeval time_tv;
    err = osc_buffer_end_time(osc, buf, &time_tv);
    if(err != E_NO_ERROR) return err;

    struct timeval period_tv;
    err = osc_sample_period(osc, &period_tv);
    if(err != E_NO_ERROR) return err;

    size_t buf_samples_count = osc_buffer_samples_count(osc, buf);

    size_t sample_count = sample + 1;
    size_t i;

    if(sample_count <= buf_samples_count){
        for(i = sample_count; i < buf_samples_count; i ++){
            timersub(&time_tv, &period_tv, &time_tv);
        }
    }else{
        for(i = buf_samples_count; i < sample_count; i ++){
            timeradd(&time_tv, &period_tv, &time_tv);
        }
    }

    tv->tv_sec = time_tv.tv_sec;
    tv->tv_usec = time_tv.tv_usec;

    return E_NO_ERROR;
}

void osc_pause(osc_t* osc, iq15_t time)
{
    osc_buffer_t* buffer = osc_buffer(osc, osc->put_buf_index);

    if(buffer->paused) return;

    lq15_t time_samples = iq15_imull(time, AIN_SAMPLE_FREQ);
    size_t samples = (size_t)IQ15_INT(time_samples);

    osc->pause_counter = 0;
    osc->pause_samples = samples;
    osc->pause_enabled = true;
}

void osc_pause_current(osc_t* osc)
{
    osc_buffer_t* buffer = osc_buffer(osc, osc->put_buf_index);

    if(buffer->paused) return;

    osc->pause_counter = 0;
    osc->pause_samples = 0;
    osc->pause_enabled = true;
}

bool osc_pause_pending(osc_t* osc)
{
    return osc->pause_enabled;
}

err_t osc_sample_period(osc_t* osc, struct timeval* tv)
{
    if(!tv) return E_NULL_POINTER;

    tv->tv_sec = 0;
    tv->tv_usec = AIN_SAMPLE_PERIOD_US * osc_rate(osc);

    return E_NO_ERROR;
}

iq15_t osc_sample_freq(osc_t* osc)
{
    return IQ15F(AIN_SAMPLE_FREQ, osc_rate(osc));
}

err_t osc_channel_set_enabled(osc_t* osc, size_t n, bool enabled)
{
	if(n >= osc->channels_count) return E_OUT_OF_RANGE;

	osc_channel_t* channel = osc_channel(osc, n);

	channel->enabled = enabled;

    return E_NO_ERROR;
}

bool osc_channel_enabled(osc_t* osc, size_t n)
{
	if(n >= osc->channels_count) return false;

	osc_channel_t* channel = osc_channel(osc, n);

	return channel->enabled;
}

err_t osc_channel_reset(osc_t* osc, size_t n)
{
	if(n >= osc->channels_count) return false;

	osc_channel_t* channel = osc_channel(osc, n);

	channel->enabled = false;
	channel->offset = OSC_INDEX_INVALID;

	if(channel->type == OSC_VAL){
	    avg_reset(&channel->avg);
	}else{
	    maj_reset(&channel->maj);
	}

	return E_NO_ERROR;
}

osc_src_t osc_channel_src(osc_t* osc, size_t n)
{
    if(n >= osc->channels_count) return 0;

    osc_channel_t* channel = osc_channel(osc, n);

    return channel->src;
}

osc_type_t osc_channel_type(osc_t* osc, size_t n)
{
    if(n >= osc->channels_count) return 0;

    osc_channel_t* channel = osc_channel(osc, n);

    return channel->type;
}

osc_src_type_t osc_channel_src_type(osc_t* osc, size_t n)
{
    if(n >= osc->channels_count) return 0;

    osc_channel_t* channel = osc_channel(osc, n);

    return channel->src_type;
}

size_t osc_channel_src_channel(osc_t* osc, size_t n)
{
    if(n >= osc->channels_count) return 0;

    osc_channel_t* channel = osc_channel(osc, n);

    return channel->src_channel;
}

err_t osc_channel_init(osc_t* osc, size_t n, osc_src_t src, osc_type_t type, osc_src_type_t src_type, size_t src_channel)
{
	if(n >= osc->channels_count) return E_OUT_OF_RANGE;

	osc_channel_t* channel = osc_channel(osc, n);

	osc_channel_reset(osc, n);

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
 * Игнорирует запрещённые каналы.
 * @param channels Каналы.
 * @param count Число семплов.
 * @return Размер.
 */
static size_t osc_channels_calc_req_size(osc_t* osc, size_t count)
{
    osc_channel_t* channel = NULL;

    size_t req_size = 0;

    size_t i;
    for(i = 0; i < osc->channels_count; i ++){
        channel = osc_channel(osc, i);

    	// Пропуск запрещённых каналов.
        if(!channel->enabled) continue;

        req_size += osc_channel_count_to_size(channel, count);
    }

    return req_size;
}

/**
 * Вычисляет необходимое число семплов для канала.
 * @param rate Коэффициент числа семплов.
 * @return Необходимое число семплов.
 */
static size_t osc_calc_channel_samples(osc_t* osc, iq15_t rate)
{
    // Необходимый размер.
    int32_t samples_count = osc->buf_samples;
    // Скорректируем.
    samples_count = iq15_imul(rate, samples_count);
    // Округлим до целого в меньшую сторону.
    samples_count = iq15_int(samples_count);

    return samples_count;
}

/**
 * Выделяет буферы данных каналов.
 * Игнорирует запрещённые каналы.
 * @param channels Каналы.
 * @param count Число каналов.
 * @param rate Относительный размер.
 * @return Код ошибки.
 */
static err_t osc_alloc_buffers(osc_t* osc, size_t samples_count)
{
    osc_channel_t* channel = NULL;
    size_t offset = OSC_INDEX_INVALID;
    int32_t size = 0;

    osc_pool_t pool;
    osc_pool_init(&pool, NULL, osc->buf_samples);

    size_t i;
    for(i = 0; i < osc->channels_count; i ++){
        channel = osc_channel(osc, i);

    	// Пропуск запрещённых каналов.
    	if(!channel->enabled) continue;

    	// Вычислим размер буфера.
    	size = osc_channel_count_to_size(channel, samples_count);

    	// Минимальный размер.
		if(size == 0) return E_INVALID_VALUE;

        // Выделим буфер данных.
		offset = osc_pool_alloc_index(&pool, size);
        // Недостаточно памяти.
        if(offset == OSC_INDEX_INVALID) return E_OUT_OF_MEMORY;

        channel->offset = offset;
    }

    return E_NO_ERROR;
}

/**
 * Вычисляет время записи осциллограмм.
 * @param osc_rate Коэффициент деления частоты дискретизации.
 * @param size_rate Относительный размер данных.
 * @return Время записи осциллограммы.
 */
static iq15_t osc_calc_time(osc_t* osc, size_t osc_rate, iq15_t size_rate)
{
    // Время буфера для осциллограмм.
    iq15_t buf_time = (iq15_t)LQ15F(osc->buf_samples, AIN_SAMPLE_FREQ);
    // Учтём снижение частоты дискретизации.
    buf_time = iq15_imul(buf_time, osc_rate);
    // Вычислим время.
    iq15_t time = iq15_mull(buf_time, size_rate);

    return time;
}

static void osc_calc_channels_types_count(osc_t* osc)
{
    osc_channel_t* channel = NULL;

    osc->enabled_channels = 0;
    osc->analog_channels = 0;
    osc->digital_channels = 0;

    size_t i;
    for(i = 0; i < osc->channels_count; i ++){
        channel = osc_channel(osc, i);
        if(channel->enabled){
            osc->enabled_channels ++;
            if(channel->type == OSC_VAL) osc->analog_channels ++;
            else osc->digital_channels ++;
        }
    }
}

err_t osc_init_channels(osc_t* osc, size_t rate)
{
    if(rate == 0) return E_INVALID_VALUE;

    err_t err = E_NO_ERROR;

    // Полный размер буфера.
    const size_t total_size = osc->buf_samples;
    // Число семплов по-умолчанию.
    size_t samples_count = osc->buf_samples;
    // Вычислим полный требуемый размер буфера.
    size_t total_req_size = osc_channels_calc_req_size(osc, samples_count);

    // Вычислим относительный размер для всех каналов.
    //iq15_t size_rate = IQ15F(total_size, total_req_size);
    // Для поддержки буферов большого объёма (больше 64кб).
    iq15_t size_rate = (iq15_t)((((int64_t)total_size) << Q15_FRACT_BITS) / total_req_size);

    // Вычислим число семплов.
    samples_count = osc_calc_channel_samples(osc, size_rate);

    // Если вдруг из-за погрешности число семплов всё ещё слишком велико,
    // будем уменьшать число семплов, пока не влезем в буфер.
    do {
        // Вычислим нужный размер.
        total_req_size = osc_channels_calc_req_size(osc, samples_count);
        // Если влезли в буфер - идём дальше.
        if(total_req_size <= total_size) break;
        // Уменьшим число семплов.
        samples_count --;
    } while(samples_count);

    // Если число выходит за пределы - ошибка.
    if(samples_count == 0 || samples_count > osc->buf_samples) return E_OUT_OF_RANGE;

    // Выделим память.
    err = osc_alloc_buffers(osc, samples_count);
    if(err != E_NO_ERROR) return err;

    // Числов каналов разного типа.
     osc_calc_channels_types_count(osc);

    // Число семплов.
    osc->samples = samples_count;

    // Дециматоры.
    decim_init(&osc->decim, rate);

    // Вычисление времени осциллограммы.
    osc->time = osc_calc_time(osc, rate, size_rate);

    return E_NO_ERROR;
}

size_t osc_enabled_channels(osc_t* osc)
{
    return osc->enabled_channels;
}

size_t osc_analog_channels(osc_t* osc)
{
    return osc->analog_channels;
}

size_t osc_digital_channels(osc_t* osc)
{
    return osc->digital_channels;
}

const char* osc_channel_name(osc_t* osc, size_t n)
{
    osc_src_t src = osc_channel_src(osc, n);
    size_t src_n = osc_channel_src_channel(osc, n);

    if(src == OSC_AIN) return ain_channel_name(src_n);
    else if(src == OSC_DIN) return din_name(src_n);

    return NULL;
}

const char* osc_channel_unit(osc_t* osc, size_t n)
{
    osc_src_t src = osc_channel_src(osc, n);
    size_t src_n = osc_channel_src_channel(osc, n);

    if(src == OSC_AIN) return ain_channel_unit(src_n);

    return NULL;
}

iq15_t osc_channel_scale(osc_t* osc, size_t n)
{
    osc_src_t src = osc_channel_src(osc, n);
    size_t src_n = osc_channel_src_channel(osc, n);

    if(src == OSC_AIN) return ain_channel_real_k(src_n);

    return IQ15I(1);
}

/**
 * Получает индекс канала по типу и номеру.
 * @param osc Осциллограмма.
 * @param type Тип канала.
 * @param n Номер канала канала заданного типа.
 * @return Индекс канала.
 */
static size_t osc_channel_type_index(osc_t* osc, osc_type_t type, size_t n)
{
    osc_channel_t* channel = NULL;
    size_t ch_cnt = 0;

    size_t i;
    for(i = 0; i < osc->channels_count; i ++){
        channel = osc_channel(osc, i);

        if(!channel->enabled) continue;

        if(channel->type == type){
            if(ch_cnt == n) return i;

            ch_cnt ++;
        }
    }
    return OSC_INDEX_INVALID;
}

size_t osc_analog_channel_index(osc_t* osc, size_t n)
{
    if(n >= osc->analog_channels) return OSC_INDEX_INVALID;

    return osc_channel_type_index(osc, OSC_VAL, n);
}

size_t osc_digital_channel_index(osc_t* osc, size_t n)
{
    if(n >= osc->digital_channels) return OSC_INDEX_INVALID;

    return osc_channel_type_index(osc, OSC_BIT, n);
}

