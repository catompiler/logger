/**
 * @file osc.h Библиотека осциллографирования.
 */

#ifndef OSC_H_
#define OSC_H_


#include "errors/errors.h"
#include "q15.h"
#include "ain.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


//! Количество осциллограмм.
#define OSC_COUNT_MAX 16

//! Максимальное число семплов на все каналы.
#define OSC_SAMPLES_MAX 1024

//! Тип значения осциллограммы.
typedef int16_t osc_value_t;

//! Тип источника.
typedef enum _Osc_Src {
    OSC_AIN = 0, //!< Аналоговый вход.
    OSC_DIN = 1 //!< Цифровой вход.
} osc_src_t;

//! Тип значения.
typedef enum _Osc_Type {
	OSC_VAL = 0, //!< Значение.
	OSC_BIT = 1, //!< Бит.
} osc_type_t;

//! Тип значения.
typedef enum _Osc_Src_Type {
    OSC_INST = 0, //!< Мгновенное значение.
    OSC_EFF = 1 //!< Действующее значение.
} osc_src_type_t;



/**
 * Инициализирует осциллограмму.
 */
extern void osc_init(void);

/**
 * Обрабатывает очередной тик АЦП.
 * Добавляет данные в осциллограммы.
 */
extern void osc_append(void);

/**
 * Устанавливает разрешение записи осциллограмм.
 * @param enabled Разрешение.
 */
extern void osc_set_enabled(bool enabled);

/**
 * Получает разрешение записи осциллограмм.
 * @return Разрешение.
 */
extern bool osc_enabled(void);

/**
 * Сбрасывает осциллограммы.
 */
extern void osc_reset(void);

/**
 * Получает число используемых каналов.
 * @return Число используемых каналов.
 */
extern size_t osc_used_channels(void);

/**
 * Устанавливает разрешение записи осциллограммы канала.
 * @param n Номер канала.
 * @param enabled Разрешение.
 * @return Код ошибки.
 */
extern err_t osc_channel_set_enabled(size_t n, bool enabled);

/**
 * Получает разрешение записи осциллограммы канала.
 * @param n Номер канала.
 * @return Разрешение.
 */
extern bool osc_channel_enabled(size_t n);

/**
 * Сбрасывает канал осциллограммы.
 * @param n Номер канала.
 * @return Код ошибки.
 */
extern err_t osc_channel_reset(size_t n);

/**
 * Получает число семплов в канале.
 * @param n Номер канала.
 * @return Число семплов.
 */
extern size_t osc_channel_samples(size_t n);

/**
 * Получает текущий индекс данных канала.
 * @param n Номер канала.
 * @return Текущий индекс данных канала.
 */
extern size_t osc_channel_index(size_t n);

/**
 * Получает следующий индекс данных канала.
 * @param n Номер канала.
 * @param index Текущий индекс.
 * @return Следующий индекс.
 */
extern size_t osc_channel_next_index(size_t n, size_t index);

/**
 * Получает данные канала.
 * @param n Номер канала.
 * @param index Индекс данных.
 * @return Данные канала.
 */
extern osc_value_t osc_channel_value(size_t n, size_t index);

/**
 * Инициализирует канал.
 * После инициализации каналов необходимо
 * распределить память между ними
 * вызовом osc_init.
 * @param n Номер канала.
 * @param src Источник данных.
 * @param type Тип значения.
 * @param src_type Тип - мгновенное или действующее.
 * @param src_channel Номер канала источника данных.
 * @param rate  Делитель частоты дискретизации.
 * @return Код ошибки.
 */
extern err_t osc_channel_init(size_t n, osc_src_t src, osc_type_t type, osc_src_type_t src_type, size_t src_channel, size_t rate);

/**
 * Распределяет между проинициализированными
 * каналами доступную память под осциллограммы.
 * @param count Число используемых каналов.
 * @return Код ошибки.
 */
extern err_t osc_init_channels(size_t count);

#endif /* OSC_H_ */
