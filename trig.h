/**
 * @file trig.h Библиотека триггеров.
 */

#ifndef TRIG_H_
#define TRIG_H_


#include <errors/errors.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "q15.h"


//! Максимум каналов.
#define TRIG_COUNT_MAX 16

//! Длина имени.
#define TRIG_NAME_LEN 16

//! Тип значения триггера.
typedef int16_t trig_value_t;

//! Тип источника значений триггера.
typedef enum _Trig_Src {
	TRIG_AIN = 0, //!< Аналоговые входа.
	TRIG_DIN = 1 //!< Цифровые входа.
} trig_src_t;

//! Тип значения источника.
typedef enum _Trig_Src_Type {
    TRIG_INST = 0, //!< Мгновенное значение.
    TRIG_EFF = 1 //!< Действующее значение.
} trig_src_type_t;

//!< Тип срабатывания триггера.
typedef enum _Trig_Type {
	TRIG_OVF = 0, //!< Превышение.
	TRIG_UDF = 1 //!< Понижение.
} trig_type_t;

//! Тип структуры инициализации триггера.
typedef struct _Trig_Init {
	trig_src_t src; //!< Источник.
	size_t src_channel; //!< Канал источника.
	trig_src_type_t src_type; //!< Тип источника.
	trig_type_t type; //!< Тип срабатывания.
	q15_t time; //!< Время срабатывания, доли секунды.
	iq15_t ref; //!< Опорное значение.
	const char* name; //!< Имя триггера.
} trig_init_t;


/**
 * Инициализирует триггеры.
 */
extern void trig_init(void);

/**
 * Сбрасывает триггеры.
 */
extern void trig_reset(void);

/**
 * Устанавливает разрешение триггеров.
 * @param enabled Разрешение триггеров.
 */
extern void trig_set_enabled(bool enabled);

/**
 * Получает разрешение триггеров.
 * @param n Номер канала.
 * @return Разрешение триггеров.
 */
extern bool trig_enabled(void);

/**
 * Проверяет триггеры.
 * @param dt Время с последней проверки.
 * @return Флаг срабатывания хотя бы одного триггера.
 */
extern bool trig_check(q15_t dt);

/**
 * Сбрасывает канал триггеров.
 * @param n Номер канала.
 * @return Код ошибки.
 */
extern err_t trig_channel_reset(size_t n);

/**
 * Инициализирует канал триггеров.
 * Источники для триггеров должны быть проинициализированы.
 * @param n Номер канала.
 * @param init Структура инициализации.
 * @return Код ошибки.
 */
extern err_t trig_channel_init(size_t n, trig_init_t* init);

/**
 * Устанавливает разрешение канала.
 * @param n Номер канала.
 * @param enabled Разрешение канала.
 * @return Код ошибки.
 */
extern err_t trig_channel_set_enabled(size_t n, bool enabled);

/**
 * Получает разрешение канала.
 * @param n Номер канала.
 * @return Разрешение канала.
 */
extern bool trig_channel_enabled(size_t n);

/**
 * Получает флаг активации канала.
 * @param n Номер канала.
 * @return Флаг активации канала.
 */
extern bool trig_channel_activated(size_t n);

/**
 * Получает имя канала.
 * @param n Номер канала.
 * @return Имя канала.
 */
extern const char* trig_channel_name(size_t n);

#endif /* TRIG_H_ */
