/**
 * @file osc.h Библиотека осциллографирования.
 */

#ifndef OSC_H_
#define OSC_H_


#include "errors/errors.h"
#include "q15.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/time.h>


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
 * Сбрасывает используемые каналы осциллограммы.
 * Устанавливает нулевое количество используемых каналов.
 * Освобождает память данных каналов.
 */
extern void osc_reset(void);

/**
 * Получает число используемых каналов.
 * @return Число используемых каналов.
 */
extern size_t osc_used_channels(void);

/**
 * Получает время записи осциллограммы.
 * @return Время записи осциллограммы.
 */
extern iq15_t osc_time(void);

/**
 * Получает коэффициент деления частоты дискретизации.
 * @return Коэффициент деления частоты дискретизации.
 */
extern size_t osc_rate(void);

/**
 * Получает число семплов в осциллограмме.
 * @return Число семплов.
 */
extern size_t osc_samples(void);

/**
 * Получает текущий индекс данных осциллограммы.
 * @return Текущий индекс.
 */
extern size_t osc_index(void);

/**
 * Получает следующий индекс данных осциллограммы.
 * @param index Текущий индекс.
 * @return Следующий индекс.
 */
extern size_t osc_next_index(size_t index);

/**
 * Получает индекс семпла по номеру.
 * @param sample Номер семпла.
 * @return Индекс.
 */
extern size_t osc_sample_index(size_t sample);

/**
 * Получает сдвиг осциллограммы по частоте АЦП.
 * @return Сдвиг в семплах частоты АЦП.
 */
extern size_t osc_skew(void);

/**
 * Делает паузу записи осциллограммы после
 * количества семплов, эквивалентное заданному времени.
 * @param time Время.
 */
extern void osc_pause(iq15_t time);

/**
 * Получает флаг паузы осциллограмм.
 * @return Флаг паузы осциллограмм.
 */
extern bool osc_paused(void);

/**
 * Продолжает запись осциллограмм.
 */
extern void osc_resume(void);

/**
 * Получает количество семплов после паузы осциллограммы.
 * @return Количество семплов после паузы осциллограммы.
 */
extern size_t osc_paused_samples(void);

/**
 * Получает время останова записи осциллограммы.
 * @param tv Время останов записи осциллограммы.
 */
extern void osc_paused_time(struct timeval* tv);

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
 * Получает источник канала.
 * @param n Номер канала.
 * @return Источник канала.
 */
extern osc_src_t osc_channel_src(size_t n);

/**
 * Получает тип канала.
 * @param n Номер канала.
 * @return Тип канала.
 */
extern osc_type_t osc_channel_type(size_t n);

/**
 * Получает тип источника канала.
 * @param n Номер канала.
 * @return Тип источника канала.
 */
extern osc_src_type_t osc_channel_src_type(size_t n);

/**
 * Получает канал источника.
 * @param n Номер канала.
 * @return Канал источника.
 */
extern size_t osc_channel_src_channel(size_t n);

/**
 * Получает данные канала.
 * @param n Номер канала.
 * @param index Индекс данных.
 * @return Данные канала.
 */
extern osc_value_t osc_channel_value(size_t n, size_t index);

/**
 * Инициализирует канал.
 * Сбрасывает настройки данных канала и
 * запрещает его.
 * Для использования канала его необходимо разрешить.
 * После инициализации каналов необходимо
 * распределить память между ними
 * вызовом osc_init.
 * @param n Номер канала.
 * @param src Источник данных.
 * @param type Тип значения.
 * @param src_type Тип - мгновенное или действующее.
 * @param src_channel Номер канала источника данных.
 * @return Код ошибки.
 */
extern err_t osc_channel_init(size_t n, osc_src_t src, osc_type_t type, osc_src_type_t src_type, size_t src_channel);

/**
 * Распределяет между проинициализированными
 * каналами доступную память под осциллограммы.
 * @param rate  Делитель частоты дискретизации.
 * @return Код ошибки.
 */
extern err_t osc_init_channels(size_t rate);

#endif /* OSC_H_ */
