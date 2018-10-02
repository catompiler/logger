#include "dout.h"
#include "defs/defs.h"


//! Структура канала цифрового выхода.
typedef struct _Dout_Channel {
	GPIO_TypeDef* gpio; //!< GPIO.
	gpio_pin_t pin; //!< Пин.
	dout_type_t type; //!< Тип.
	dout_state_t state; //!< Состояние выхода.
} dout_channel_t;


//! Структура цифровых выходов.
typedef struct _Dout {
	dout_channel_t channels[DOUT_COUNT]; //!< Каналы.
} dout_t;

//! Цифровые выхода.
static dout_t dout;



//! Получает канал цифрового выхода по номеру.
ALWAYS_INLINE static dout_channel_t* dout_channel(size_t n)
{
	return &dout.channels[n];
}

//! Получает текущее состояние выхода.
ALWAYS_INLINE static err_t dout_channel_set_state_inst(dout_channel_t* channel, dout_state_t state)
{
	if(channel->gpio == NULL || channel->pin == 0) return E_STATE;

	gpio_state_t inst_state = GPIO_STATE_OFF;

	if(state == DOUT_ON){
		inst_state = (channel->type == DOUT_NORMAL) ? GPIO_STATE_ON : GPIO_STATE_OFF;
	}else{
		inst_state = (channel->type == DOUT_NORMAL) ? GPIO_STATE_OFF : GPIO_STATE_ON;
	}

	if(inst_state == GPIO_STATE_ON){
		gpio_set(channel->gpio, channel->pin);
	}else{
		gpio_reset(channel->gpio, channel->pin);
	}

	channel->state = state;

	return E_NO_ERROR;
}


err_t dout_init(void)
{
	memset(&dout, 0x0, sizeof(dout_t));

	return E_NO_ERROR;
}

err_t dout_channel_init(size_t n, GPIO_TypeDef* gpio, gpio_pin_t pin)
{
	if(n >= DOUT_COUNT) return E_OUT_OF_RANGE;
	if(gpio == NULL || pin == 0) return E_INVALID_VALUE;

	dout_channel_t* channel = dout_channel(n);

	channel->gpio = gpio;
	channel->pin = pin;

	return E_NO_ERROR;
}

err_t dout_channel_setup(size_t n, dout_type_t type)
{
	if(n >= DOUT_COUNT) return E_OUT_OF_RANGE;

	dout_channel_t* channel = dout_channel(n);

	channel->type = type;

	return E_NO_ERROR;
}

void dout_process(void)
{
	dout_channel_t* channel = NULL;

	size_t i;
	for(i = 0; i < DOUT_COUNT; i ++){
		channel = dout_channel(i);

		dout_channel_set_state_inst(channel, channel->state);
	}
}

err_t dout_set_state(size_t n, dout_state_t state)
{
	if(n >= DOUT_COUNT) return E_OUT_OF_RANGE;

	dout_channel_t* channel = dout_channel(n);

	channel->state = state;

	return E_NO_ERROR;
}

