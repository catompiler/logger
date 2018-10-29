/**
 * @file osc.h Библиотека осциллографирования.
 */

#ifndef OSC_H_
#define OSC_H_


#include "errors/errors.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/time.h>
#include "q15.h"
#include "decim.h"
#include "avg.h"
#include "maj.h"


//! Неправильный индекс.
#define OSC_INDEX_INVALID 0xffffffff

//! Количество осциллограмм.
//#define OSC_COUNT_MAX 16

//! Максимальное число семплов на все каналы.
//#define OSC_SAMPLES_MAX 1024

//! Тип значения осциллограммы.
typedef int16_t osc_value_t;

//! Число цифровых сигналов в одном семпле.
#define OSC_BITS_PER_SAMPLE (sizeof(osc_value_t) * 8)

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


//! Тип режима переключения буферов.
typedef enum _Osc_Buffer_Mode {
    OSC_RING_IN_BUFFER = 0, //!< Кольцо внутри буфера.
    OSC_BUFFER_IN_RING = 1 //!< Кольцо из всех буферов.
} osc_buffer_mode_t;

//! Структура буфера данных осциллограммы.
typedef struct _Osc_Buffer {
    osc_value_t* data; //!< Данные буфера.
    size_t size; //!< Размер буфера.
    size_t index; //!< Текущий индекс.
    size_t count; //! Число значений в буфере.
    struct timeval end_time; //!< Время последнего семпла.
    bool paused; //!< Флаг паузы записи.
} osc_buffer_t;

//! Структура канала осциллограммы.
typedef struct _Osc_Channel {
    osc_src_t src; //!< Источник данных.
    osc_type_t type; //!< Тип значения.
    osc_src_type_t src_type; //!< Тип - мгновенное или действующее.
    size_t src_channel; //!< Номер канала источника данных.
    union {
        avg_t avg; //!< Усреднение.
        maj_t maj; //!< Мажоритар.
    };
    size_t offset; //!< Данные.
    bool enabled; //!< Разрешение канала.
} osc_channel_t;

//! Пул данных осциллограмм.
typedef struct _Osc_Pool {
    osc_value_t* data; //!< Буфер данных пула.
    size_t size; //!< Размер данных в пуле.
    size_t index; //!< Индекс свободного элемента.
} osc_pool_t;

//! Структура осциллограмм.
typedef struct _Osc {
    // Данные инициализации.
    osc_value_t* data; //!< Данные осциллограммы.
    size_t data_size; //!< Размер данных.
    osc_buffer_t* buffers; //!< Буфера.
    size_t buffers_count; //!< Количество буферов.
    osc_channel_t* channels; //!< Каналы осциллограммы.
    size_t channels_count; //!< Количество каналов в буфере.
    // Индексы буферов.
    size_t put_buf_index; //!< Индекс буфера для записи.
    size_t get_buf_index; //!< Индекс буфера для чтения.
    // Параметры данных в буферах.
    size_t buf_samples; //!< Максимальное число семплов в буфере.
    size_t samples; //!< Число используемых семплов в бефере.
    // Общие данные времени выполнения.
    osc_buffer_mode_t buffer_mode; //!< Режим буферов.
    bool enabled; //!< Разрешение каналов.
    decim_t decim; //!< Дециматор.
    iq15_t time; //!< Время осциллограммы.
    // Пауза.
    bool pause_enabled; //!< Разрешение паузы.
    size_t pause_counter; //!< Счётчик семплов до паузы.
    size_t pause_samples; //!< Число семплов до паузы.
    // Вычисленные данные на момент инициализации.
    size_t enabled_channels; //!< Разрешённые каналы.
    size_t analog_channels; //!< Число аналоговых каналов.
    size_t digital_channels; //!< Число цифровых каналов.
} osc_t;


/**
 * Инициализирует осциллограмму.
 * @param osc Осциллограмма.
 * @param data Буфер данных.
 * @param data_size размер буфера данных.
 * @param buffers Буферы данных.
 * @param buffers_count Число буферов данных.
 * @param channels Каналы осциллограммы.
 * @param channels_count Число каналов.
 * @return Код ошибки.
 */
extern err_t osc_init(osc_t* osc, osc_value_t* data, size_t data_size,
                      osc_buffer_t* buffers, size_t buffers_count,
                      osc_channel_t* channels, size_t channels_count);

/**
 * Получает режим буферов осциллограммы.
 * @param osc Осциллограмма.
 * @return Режим буферов.
 */
extern osc_buffer_mode_t osc_buffer_mode(osc_t* osc);

/**
 * Устанавливает режим буферов осциллограммы.
 * @param osc Осциллограмма.
 * @param mode Режим буферов осциллограммы.
 */
extern void osc_set_buffer_mode(osc_t* osc, osc_buffer_mode_t mode);

/**
 * Обрабатывает очередной тик АЦП.
 * Добавляет данные в осциллограммы.
 * @param osc Осциллограмма.
 */
extern void osc_append(osc_t* osc);

/**
 * Устанавливает разрешение записи осциллограмм.
 * @param osc Осциллограмма.
 * @param enabled Разрешение.
 */
extern void osc_set_enabled(osc_t* osc, bool enabled);

/**
 * Получает разрешение записи осциллограмм.
 * @param osc Осциллограмма.
 * @return Разрешение.
 */
extern bool osc_enabled(osc_t* osc);

/**
 * Сбрасывает используемые каналы осциллограммы.
 * Устанавливает нулевое количество используемых каналов.
 * Освобождает память данных каналов.
 * @param osc Осциллограмма.
 */
extern void osc_reset(osc_t* osc);

/**
 * Получает время записи осциллограммы.
 * @param osc Осциллограмма.
 * @return Время записи осциллограммы.
 */
extern iq15_t osc_time(osc_t* osc);

/**
 * Получает коэффициент деления частоты дискретизации.
 * @param osc Осциллограмма.
 * @return Коэффициент деления частоты дискретизации.
 */
extern size_t osc_rate(osc_t* osc);

/**
 * Получает число каналов.
 * @param osc Осциллограмма.
 * @return Число каналов.
 */
extern size_t osc_channels_count(osc_t* osc);

/**
 * Получает число семплов в осциллограмме.
 * @param osc Осциллограмма.
 * @return Число семплов.
 */
extern size_t osc_samples_count(osc_t* osc);

/**
 * Получает число буферов осциллограммы.
 * @param osc Осциллограмма.
 * @return Число буферов.
 */
extern size_t osc_buffers_count(osc_t* osc);

/**
 * Получает текущий буфер чтения осциллограмм.
 * @param osc Осциллограмма.
 * @return Текущий буфер чтения.
 */
extern size_t osc_current_buffer(osc_t* osc);

/**
 * Переходит на следующий буфер чтения.
 * @param osc Осциллограмма.
 * @return Новый индекс текущего буфера чтения.
 */
extern size_t osc_next_buffer(osc_t* osc);

/**
 * Получает индекс следующего буфера.
 * @param osc Осциллограмма.
 * @return Индекс следующего буфера.
 */
extern size_t osc_next_buffer_index(osc_t* osc, size_t buf);

/**
 * Получает число семплов заданного буфера.
 * @param osc Осциллограмма.
 * @param buf Индекс буфера.
 * @return Число семплов буфера.
 */
extern size_t osc_buffer_samples_count(osc_t* osc, size_t buf);

/**
 * Получает текущий индекс семпла заданного буфера.
 * @param osc Осциллограмма.
 * @param buf Индекс буфера.
 * @return Текущий индекс семпла буфера.
 */
extern size_t osc_buffer_sample_index(osc_t* osc, size_t buf);

/**
 * Переходит на следующий семпл в буфере данных осциллограммы.
 * @param osc Осциллограмма.
 * @param buf Буфер данных.
 * @return Следующий индекс.
 */
extern size_t osc_buffer_next_sample(osc_t* osc, size_t buf);

/**
 * Получает следующий индекс семпла данных осциллограммы.
 * @param osc Осциллограмма.
 * @param index Текущий индекс.
 * @return Следующий индекс.
 */
extern size_t osc_next_sample_index(osc_t* osc, size_t index);

/**
 * Получает индекс семпла в буфере по номеру.
 * @param osc Осциллограмма.
 * @param buf Буфер данных.
 * @param sample Номер семпла.
 * @return Индекс.
 */
extern size_t osc_buffer_sample_number_index(osc_t* osc, size_t buf, size_t sample);

/**
 * Получает данные канала.
 * @param osc Осциллограмма.
 * @param buf Буфер данных.
 * @param n Номер канала.
 * @param index Индекс данных.
 * @return Данные канала.
 */
extern osc_value_t osc_buffer_channel_value(osc_t* osc, size_t buf, size_t n, size_t index);

/**
 * Получает флаг паузы буфера осциллограммы.
 * @param osc Осциллограмма.
 * @param buf Буфер данных.
 * @return Флаг паузы осциллограмм.
 */
extern bool osc_buffer_paused(osc_t* osc, size_t buf);

/**
 * Продолжает запись осциллограмм в буфер.
 * Сбрасывает буфер.
 * @param osc Осциллограмма.
 * @param buf Буфер данных.
 */
extern void osc_buffer_resume(osc_t* osc, size_t buf);

/**
 * Получает время конца осциллограммы в буфере.
 * @param osc Осциллограмма.
 * @param buf Буфер данных.
 * @param tv Время.
 * @return Код ошибки.
 */
extern err_t osc_buffer_end_time(osc_t* osc, size_t buf, struct timeval* tv);

/**
 * Получает время начала осциллограммы в буфере.
 * @param osc Осциллограмма.
 * @param buf Буфер данных.
 * @param tv Время.
 * @return Код ошибки.
 */
extern err_t osc_buffer_start_time(osc_t* osc, size_t buf, struct timeval* tv);

/**
 * Делает паузу записи осциллограммы в текущий буфер после
 * количества семплов, эквивалентное заданному времени.
 * @param osc Осциллограмма.
 * @param time Время.
 */
extern void osc_pause(osc_t* osc, iq15_t time);

/**
 * Получает время (период) семпла осциллограммы.
 * @param osc Осциллограмма.
 * @param tv Время.
 * @return Код ошибки.
 */
extern err_t osc_sample_period(osc_t* osc, struct timeval* tv);

/**
 * Устанавливает разрешение записи осциллограммы канала.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @param enabled Разрешение.
 * @return Код ошибки.
 */
extern err_t osc_channel_set_enabled(osc_t* osc, size_t n, bool enabled);

/**
 * Получает разрешение записи осциллограммы канала.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Разрешение.
 */
extern bool osc_channel_enabled(osc_t* osc, size_t n);

/**
 * Сбрасывает канал осциллограммы.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Код ошибки.
 */
extern err_t osc_channel_reset(osc_t* osc, size_t n);

/**
 * Получает источник канала.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Источник канала.
 */
extern osc_src_t osc_channel_src(osc_t* osc, size_t n);

/**
 * Получает тип канала.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Тип канала.
 */
extern osc_type_t osc_channel_type(osc_t* osc, size_t n);

/**
 * Получает тип источника канала.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Тип источника канала.
 */
extern osc_src_type_t osc_channel_src_type(osc_t* osc, size_t n);

/**
 * Получает канал источника.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Канал источника.
 */
extern size_t osc_channel_src_channel(osc_t* osc, size_t n);

/**
 * Инициализирует канал.
 * Сбрасывает настройки данных канала и
 * запрещает его.
 * Для использования канала его необходимо разрешить.
 * После инициализации каналов необходимо
 * распределить память между ними
 * вызовом osc_init.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @param src Источник данных.
 * @param type Тип значения.
 * @param src_type Тип - мгновенное или действующее.
 * @param src_channel Номер канала источника данных.
 * @return Код ошибки.
 */
extern err_t osc_channel_init(osc_t* osc, size_t n, osc_src_t src, osc_type_t type, osc_src_type_t src_type, size_t src_channel);

/**
 * Распределяет между проинициализированными
 * каналами доступную память под осциллограммы.
 * @param osc Осциллограмма.
 * @param rate  Делитель частоты дискретизации.
 * @return Код ошибки.
 */
extern err_t osc_init_channels(osc_t* osc, size_t rate);

/**
 * Получает число разрешённых каналов.
 * @param osc Осциллограмма.
 * @return Число разрешённых каналов.
 */
extern size_t osc_enabled_channels(osc_t* osc);

/**
 * Получает число аналоговых каналов.
 * @param osc Осциллограмма.
 * @return Число аналоговых каналов.
 */
extern size_t osc_analog_channels(osc_t* osc);

/**
 * Получает число цифровых каналов.
 * @param osc Осциллограмма.
 * @return Число цифровых каналов.
 */
extern size_t osc_digital_channels(osc_t* osc);

/**
 * Получает имя канала источника канала осциллограммы.
 * Может возвращать NULL.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Имя канала.
 */
extern const char* osc_channel_name(osc_t* osc, size_t n);

/**
 * Получает единицу измерения источника канала осциллограммы.
 * Может возвращать NULL.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Единица измерения.
 */
extern const char* osc_channel_unit(osc_t* osc, size_t n);

/**
 * Получает коэффициент преобразования в реальную величину источника канала осциллограммы.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Коэффициент преобразования.
 */
extern iq15_t osc_channel_scale(osc_t* osc, size_t n);

/**
 * Получает индекс канала по номеру аналогового канала.
 * Может возвратить OSC_INDEX_INVALID.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Индекс канала по номеру аналогового канала.
 */
extern size_t osc_analog_channel_index(osc_t* osc, size_t n);

/**
 * Получает индекс канала по номеру цифрового канала.
 * Может возвратить OSC_INDEX_INVALID.
 * @param osc Осциллограмма.
 * @param n Номер канала.
 * @return Индекс канала по номеру цифрового канала.
 */
extern size_t osc_digital_channel_index(osc_t* osc, size_t n);

#endif /* OSC_H_ */
