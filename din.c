#include "din.h"
#include "defs/defs.h"
#include <string.h>


//! Размер буфера имени.
#define DIO_NAME_BUF_SIZE (DIN_NAME_LEN + 1)


//! Структура канала цифрового входа.
typedef struct _Din_Channel {
	GPIO_TypeDef* gpio; //!< GPIO.
	gpio_pin_t pin; //!< Пин.
	din_type_t type; //!< Тип.
	q15_t time; //!< Время для изменения, доли секунды.
	char name[DIO_NAME_BUF_SIZE]; //!< Имя цифрового входа.
	q15_t cur_time; //!< Время с последнего изменения, доли секунды.
	bool changed; //!< Флаг изменения с последней проверки.
	din_state_t state; //!< Состояние входа.
} din_channel_t;


//! Структура цифровых входов.
typedef struct _Din {
	din_channel_t channels[DIN_COUNT]; //!< Каналы.
} din_t;

//! Цифровые входа.
static din_t din;



//! Получает канал цифрового входа по номеру.
ALWAYS_INLINE static din_channel_t* din_channel(size_t n)
{
	return &din.channels[n];
}

//! Получает текущее состояние входа.
ALWAYS_INLINE static din_state_t din_channel_state_inst(din_channel_t* channel)
{
	if(channel->gpio == NULL || channel->pin == 0) return DIN_OFF;

	if(gpio_input_state(channel->gpio, channel->pin) == GPIO_STATE_ON){
		return (channel->type == DIN_NORMAL) ? DIN_ON : DIN_OFF;
	}
	return (channel->type == DIN_NORMAL) ? DIN_OFF : DIN_ON;
}


err_t din_init(void)
{
	memset(&din, 0x0, sizeof(din_t));

	return E_NO_ERROR;
}

err_t din_channel_init(size_t n, GPIO_TypeDef* gpio, gpio_pin_t pin)
{
	if(n >= DIN_COUNT) return E_OUT_OF_RANGE;
	if(gpio == NULL || pin == 0) return E_INVALID_VALUE;

	din_channel_t* channel = din_channel(n);

	channel->gpio = gpio;
	channel->pin = pin;

	return E_NO_ERROR;
}

err_t din_channel_setup(size_t n, din_type_t type, q15_t time, const char* name)
{
	if(n >= DIN_COUNT) return E_OUT_OF_RANGE;
	if(time <= 0) return E_INVALID_VALUE;

	din_channel_t* channel = din_channel(n);

	channel->type = type;
	channel->time = time;

	if(name){
        size_t len = strlen(name);

        if(len >= DIN_NAME_LEN) len = DIN_NAME_LEN;

        memcpy(channel->name, name, len);

        channel->name[len] = '\0';
	}else{
	    channel->name[0] = '\0';
	}

	return E_NO_ERROR;
}

void din_process(q15_t dt)
{
	din_channel_t* channel = NULL;
	din_state_t state = DIN_OFF;

	size_t i;
	for(i = 0; i < DIN_COUNT; i ++){
		channel = din_channel(i);

		state = din_channel_state_inst(channel);

		channel->changed = false;

		if(channel->state != state){
			channel->cur_time = q15_add_sat(channel->cur_time, dt);

			if(channel->cur_time >= channel->time){
				channel->state = state;
				channel->cur_time = 0;
				channel->changed = true;
			}
		}else{
			channel->cur_time = 0;
		}
	}
}

din_state_t din_state(size_t n)
{
	if(n >= DIN_COUNT) return DIN_OFF;

	din_channel_t* channel = din_channel(n);

	return channel->state;
}

bool din_changed(size_t n)
{
	if(n >= DIN_COUNT) return false;

	din_channel_t* channel = din_channel(n);

	return channel->changed;
}

const char* din_name(size_t n)
{
    if(n >= DIN_COUNT) return NULL;

    din_channel_t* channel = din_channel(n);

    return channel->name;
}

