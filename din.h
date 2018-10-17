/**
 * @file din.h Библиотека цифровых входов.
 */

#ifndef DIN_H_
#define DIN_H_

#include "errors/errors.h"
#include "gpio/gpio.h"
#include <stddef.h>
#include <stdbool.h>
#include "q15.h"


//! Число цифровых входов.
#define DIN_COUNT 5

//! Число символов имени.
#define DIN_NAME_LEN 16


//! Состояние входа.
typedef enum _Din_State {
	DIN_OFF = 0, //!< Выключен.
	DIN_ON = 1, //!< Включен.
} din_state_t;

//! Тип входа - инверсия.
typedef enum _Din_Type {
	DIN_NORMAL = 0, //!< Без инверсии.
	DIN_INVERTED = 1//!< Инверсия.
} din_type_t;


/**
 * Инициализирует цифровые входа.
 */
extern err_t din_init(void);

/**
 * Инициализирует канал входа.
 * @param n Номер канала.
 * @param gpio Порт.
 * @param pin Пин.
 * @return Код ошибки.
 */
extern err_t din_channel_init(size_t n, GPIO_TypeDef* gpio, gpio_pin_t pin);

/**
 * Настраивает канал.
 * @param n Номер канала.
 * @param type Тип.
 * @param time Время установления значения, доли секунды.
 * @param name Имя канала.
 * @return Код ошибки.
 */
extern err_t din_channel_setup(size_t n, din_type_t type, q15_t time, const char* name);

/**
 * Проверяет входа.
 * @param dt Время с последней проверки.
 */
extern void din_process(q15_t dt);

/**
 * Получает состояние канала входа.
 * @param n Номер канала.
 * @return Состояние.
 */
extern din_state_t din_state(size_t n);

/**
 * Получает флаг изменения состояния канала входа.
 * @param n Номер канала.
 * @return Флаг изменения.
 */
extern bool din_changed(size_t n);

/**
 * Получает имя канала входа.
 * @param n Номер канала.
 * @return Имя.
 */
extern const char* din_name(size_t n);

#endif /* DIN_H_ */
