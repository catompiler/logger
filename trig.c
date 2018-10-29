#include "trig.h"
#include <string.h>
#include "ain.h"
#include "din.h"



//! Размер буфера для имени.
#define TRIG_NAME_BUF_SIZE ((TRIG_NAME_LEN) + 1)

//! Тип функции проверки выхода значения за допустимые пределы.
//typedef bool (*trig_check_t)(struct _Trig_Channel* channel);

//! Тип элемента триггера.
typedef struct _Trig_Channel {
	trig_src_t src; //!< Источник.
	size_t src_channel; //!< Канал источника.
	trig_src_type_t src_type; //!< Тип источника.
	trig_type_t type; //!< Тип срабатывания.
	q15_t time; //!< Время срабатывания, доли секунды.
	trig_value_t ref; //!< Опорное значение.
	char name[TRIG_NAME_BUF_SIZE]; //!< Имя триггера.
	bool enabled; //!< Разрешение канала.
	// Данные времени выполнения.
	q15_t cur_time; //!< Текущее время выхода за пределы опорного значения.
	bool activated; //!< Срабатывание триггера при последней проверке.
	bool active; //!< Срабатывание триггера.
	bool fail; //!< Нахождение значения вне пределов опорного значения.
} trig_channel_t;

//! Тип триггеров.
typedef struct _Trig {
	trig_channel_t channels[TRIG_COUNT_MAX]; //!< Каналы.
	bool enabled; //!< Разрешение триггеров.
} trig_t;

//! Триггеры.
static trig_t trig;



//! Получает канал по номеру.
ALWAYS_INLINE static trig_channel_t* trig_channel(size_t n)
{
    return &trig.channels[n];
}

static trig_value_t trig_channel_value(trig_channel_t* channel)
{
	switch(channel->src){
	case TRIG_AIN:
		if(channel->src_type == TRIG_INST){
			return ain_value_inst(channel->src_channel);
		}// TRIG_EFF
		return ain_value(channel->src_channel);

	case TRIG_DIN:
		if(channel->src_type == TRIG_INST){
			return din_state(channel->src_channel);
		}
		return din_changed(channel->src_channel) &&
		       (din_state(channel->src_channel) == DIN_ON);

	default:
		break;
	}
	return 0;
}

static bool trig_channel_compare(trig_channel_t* channel, trig_value_t value)
{
	if(channel->type == TRIG_OVF){
		return value > channel->ref;
	}
	return value < channel->ref;
}

static bool trig_check_channel(trig_channel_t* channel, q15_t dt)
{
	if(!channel->enabled) return false;

	trig_value_t value = trig_channel_value(channel);

	bool fail = trig_channel_compare(channel, value);

	bool activated = false;

	if(fail){
		channel->cur_time = q15_add_sat(channel->cur_time, dt);

		if(channel->cur_time >= channel->time){
			channel->cur_time = channel->time;

			if(channel->active == false) activated = true;

			channel->active = true;
		}
	}else{
		channel->active = false;
	}

	channel->fail = fail;
	channel->activated = activated;

	return channel->activated;
}


void trig_init(void)
{
	memset(&trig, 0x0, sizeof(trig_t));
}

void trig_reset(void)
{
	size_t i;
	for(i = 0; i < TRIG_COUNT_MAX; i ++){
		trig_channel_reset(i);
	}
}

void trig_set_enabled(bool enabled)
{
	trig.enabled = enabled;
}

bool trig_enabled(void)
{
	return trig.enabled;
}

bool trig_check(q15_t dt)
{
	if(!trig.enabled) return false;

	trig_channel_t* channel = NULL;

	bool activated = false;

	size_t i;
	for(i = 0; i < TRIG_COUNT_MAX; i ++){
		channel = trig_channel(i);

		activated |= trig_check_channel(channel, dt);
	}

	return activated;
}

err_t trig_channel_reset(size_t n)
{
	if(n >= TRIG_COUNT_MAX) return E_OUT_OF_RANGE;

	trig_channel_t* channel = trig_channel(n);

	channel->enabled = false;
	channel->activated = false;
	channel->active = false;
	channel->fail = false;
	channel->cur_time = 0;

	return E_NO_ERROR;
}

static void trig_channel_update_ref(trig_channel_t* channel)
{
    // Для аналоговых входов.
    if(channel->src == TRIG_AIN){
        // Необходимо преобразовать абсолютное значение
        iq15_t scale = ain_channel_real_k(channel->src_channel);
        // в относительное.
        channel->ref = iq15_divl(channel->ref, scale);
    }
}

err_t trig_channel_init(size_t n, trig_init_t* init)
{
	if(n >= TRIG_COUNT_MAX) return E_OUT_OF_RANGE;

	trig_channel_t* channel = trig_channel(n);

	trig_channel_reset(n);

	channel->src = init->src;
	channel->src_channel = init->src_channel;
	channel->src_type = init->src_type;
	channel->type = init->type;
	channel->time = init->time;
	channel->ref = init->ref;

	trig_channel_update_ref(channel);

	if(init->name){
        size_t len = strlen(init->name);

        if(len >= TRIG_NAME_LEN) len = TRIG_NAME_LEN;

        memcpy(channel->name, init->name, len);

        channel->name[len] = '\0';
	}else{
	    channel->name[0] = '\0';
	}

	return E_NO_ERROR;
}

err_t trig_channel_set_enabled(size_t n, bool enabled)
{
	if(n >= TRIG_COUNT_MAX) return E_OUT_OF_RANGE;

	trig_channel_t* channel = trig_channel(n);

	channel->enabled = enabled;

	return E_NO_ERROR;
}

bool trig_channel_enabled(size_t n)
{
	if(n >= TRIG_COUNT_MAX) return false;

	trig_channel_t* channel = trig_channel(n);

	return channel->enabled;
}

bool trig_channel_activated(size_t n)
{
	if(n >= TRIG_COUNT_MAX) return false;

	trig_channel_t* channel = trig_channel(n);

	return channel->activated;
}

const char* trig_channel_name(size_t n)
{
	if(n >= TRIG_COUNT_MAX) return NULL;

	trig_channel_t* channel = trig_channel(n);

	return channel->name;
}

