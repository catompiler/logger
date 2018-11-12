/**
 * @file comtrade.h Библиотека записи в формат COMTRADE.
 */

#ifndef COMTRADE_H_
#define COMTRADE_H_


#include "errors/errors.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/time.h>
#include "q15.h"
#include "fatfs/ff.h"


//! Год стандарта.
#define COMTRADE_STANDARD_YEAR 1999

//! Тип файла данных.
#define COMTRADE_DAT_FILE_TYPE "BINARY"

//! Минимальное значение данных.
#define COMTRADE_DAT_MIN (-32767)

//! Максимальное значение данных.
#define COMTRADE_DAT_MAX (32767)

//! Несуществующее значние.
#define COMTRADE_UNKNOWN_VALUE 0x8000

//! Значение первичной величины.
#define COMTRADE_PS_PRIMARY 'p'

//! Значение вторичной величины.
#define COMTRADE_PS_SECONDARY 's'


//! Структура описания аналогового канала.
typedef struct _Comtrade_Analog_Channel {
    const char* ch_id; //!< Идентификатор канала.
    const char* ph; //!< Идентификатор фазы.
    const char* ccbm; //!< Компонент схемы.
    const char* uu; //!< Единица измерения.
    iq15_t a; //!< Коэффициент значения.
    iq15_t b; //!< Смещение значения.
    uint32_t skew; //!< Сдвиг значения по времени, мкс.
    int16_t min; //!< Минимум значения.
    int16_t max; //!< Максимум значения.
    iq15_t primary; //! Соотношение первичной обмотки трансформатора.
    iq15_t secondary; //! Соотношение вторичной обмотки трансформатора.
    char ps; //!< Идентификатор пересчёта данных.
} comtrade_analog_channel_t;


//! Структура описания цифрового канала.
typedef struct _Comtrade_Digital_Channel {
    const char* ch_id; //!< Идентификатор канала.
    const char* ph; //!< Идентификатор фазы.
    const char* ccbm; //!< Компонент схемы.
    bool y; //!< Нормальное состояние канала.
} comtrade_digital_channel_t;

//! Структура описания частоты дискретизации.
typedef struct _Comtrade_Sample_Rate {
    iq15_t samp; //!< Частота.
    uint32_t endsamp; //!< Номер последнего семпла.
} comtrade_sample_rate_t;

//! Тип данных осциллограммы комтрейд.
typedef void* comtrade_osc_data_t;

//! Тип комтрейда.
typedef struct _Comtrade comtrade_t;

/**
 * Каллбэк получения описания аналогового канала.
 * @param comtrade Комтрейд.
 * @param index Номер канала.
 * @param channel Описание канала.
 */
typedef void (*comtrade_get_analog_channel_t)(comtrade_t* comtrade, size_t index, comtrade_analog_channel_t* channel);

/**
 * Каллбэк получения описания цифрового канала.
 * @param comtrade Комтрейд.
 * @param index Номер канала.
 * @param channel Описание канала.
 */
typedef void (*comtrade_get_digital_channel_t)(comtrade_t* comtrade, size_t index, comtrade_digital_channel_t* channel);

/**
 * Каллбэк получения описания частоты дискретизации.
 * @param comtrade Комтрейд.
 * @param sample Номер частоты дискретизации.
 * @param rate Описание частота дискретизации.
 */
typedef void (*comtrade_get_sample_rate_t)(comtrade_t* comtrade, size_t index, comtrade_sample_rate_t* rate);


/**
 * Каллбэк получения отметки времени семпла.
 * @param comtrade Комтрейд.
 * @param n Номер семпла.
 * @return Отметка времени.
 */
//typedef uint32_t (*comtrade_get_sample_timestamp_t)(comtrade_t* comtrade, size_t n);

/**
 * Каллбэк получения значения аналогового канала.
 * @param comtrade Комтрейд.
 * @param index Индекс канала.
 * @param sample Номер семпла.
 * @return Значение канала.
 */
typedef int16_t (*comtrade_get_analog_channel_value_t)(comtrade_t* comtrade, size_t index, size_t sample);

/**
 * Каллбэк получения значения цифрового канала.
 * @param comtrade Комтрейд.
 * @param index Индекс канала.
 * @param sample Номер семпла.
 * @return Значение канала.
 */
typedef bool (*comtrade_get_digital_channel_value_t)(comtrade_t* comtrade, size_t index, size_t sample);


//!< Структура COMTRADE.
typedef struct _Comtrade {
    // Конфигурация COMTRADE.
    const char* station_name; //!< Имя станции.
    const char* rec_dev_id; //!< Идентификатор записывающего устройства.
    //uint16_t rev_year; //!< Год версии стандарта - всегда 1999.
    size_t analog_channels; //!< Число аналоговых каналов.
    comtrade_get_analog_channel_t get_analog_channel; //!< Получение данных об аналоговом канале.
    size_t digital_channels; //!< Число цифровых каналов.
    comtrade_get_digital_channel_t get_digital_channel; //!< Получение данных о цифровом канале.
    iq15_t lf; //!< Частота сети.
    uint32_t nrates; //!< Число частот дискретизации.
    comtrade_get_sample_rate_t get_sample_rate; //!< Получение данных о частоте дискретизации.
    struct timeval data_time; //!< Время первых данных в файле.
    struct timeval trigger_time; //!< Время события.
    // comtrade_file_type_t file_type; //!< Тип файла - всегда бинарный.
    uint32_t timemult; //!< Множитель отметки времени, мкс.
    // Данные COMTRADE.
    comtrade_osc_data_t osc_data; //!< Данные осциллограммы.
    void* user_data; //!< Пользовательские данные.
    //comtrade_get_sample_timestamp_t get_sample_timestamp; //!< Получение отметки времени.
    comtrade_get_analog_channel_value_t get_analog_channel_value; //!< Получение значения аналогового канала.
    comtrade_get_digital_channel_value_t get_digital_channel_value; //!< Получение значения цифрового канала.
    // Данные времени выполнения.
} comtrade_t;


/**
 * Записывает CFG файл COMTRADE.
 * @param f Файл (*.cfg).
 * @param comtrade Комтрейд.
 * @return Код ошибки.
 */
extern err_t comtrade_write_cfg(FIL* f, comtrade_t* comtrade);

/**
 * Добавляет данные в DAT файл COMTRADE.
 * @param f Файл (*.dat).
 * @param comtrade Комтрейд.
 * @return Код ошибки.
 */
extern err_t comtrade_append_dat(FIL* f, comtrade_t* comtrade, uint32_t sample_index, uint32_t timestamp);

/**
 * Получает размер записи данных в файле.
 * @param comtrade Комтрейд.
 * @return Размер записи.
 */
extern size_t comtrade_dat_record_size(comtrade_t* comtrade);

#endif /* COMTRADE_H_ */
