/**
 * @file dout.h Библиотека цифровых выходов.
 */

#ifndef DOUT_H_
#define DOUT_H_

#include "errors/errors.h"
#include "gpio/gpio.h"
#include <stddef.h>
#include <stdbool.h>
#include "q15.h"


//! Число цифровых выходов.
#define DOUT_COUNT 4


//! Состояние выхода.
typedef enum _Dout_State {
	DOUT_OFF = 0, //!< Выключен.
	DOUT_ON = 1, //!< Включен.
} dout_state_t;

//! Режим выхода - инверсия.
typedef enum _Dout_Mode {
	DOUT_NORMAL = 0, //!< Без инверсии.
	DOUT_INVERTED = 1//!< Инверсия.
} dout_mode_t;

//! Тип выхода.
typedef enum _Dout_Type {
    DOUT_NONE = 0, //!< Отсутствует.
    DOUT_RUN = 1, //!< Работа.
    DOUT_ERROR = 2, //!< Ошибка.
    DOUT_EVENT = 3 //!< Событие.
} dout_type_t;

/**
 * Инициализирует цифровые выхода.
 */
extern err_t dout_init(void);

/**
 * Инициализирует канал выхода.
 * @param n Номер канала.
 * @param gpio Порт.
 * @param pin Пин.
 * @return Код ошибки.
 */
extern err_t dout_channel_init(size_t n, GPIO_TypeDef* gpio, gpio_pin_t pin);

/**
 * Настраивает канал.
 * @param n Номер канала.
 * @param type Тип.
 * @return Код ошибки.
 */
extern err_t dout_channel_setup(size_t n, dout_mode_t mode, dout_type_t type);

/**
 * Устанавливает выхода.
 */
extern void dout_process(void);

/**
 * Устанавливает состояние канала выхода.
 * @param n Номер канала.
 * @param state Состояние.
 * @return Код ошибки.
 */
extern err_t dout_set_state(size_t n, dout_state_t state);

/**
 * Устанавливает состояние канала выхода по типу.
 * @param type Тип.
 * @param state Состояние.
 */
extern void dout_set_type_state(dout_type_t type, dout_state_t state);

#endif /* DOUT_H_ */
