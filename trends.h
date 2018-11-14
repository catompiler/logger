/**
 * @file trends.h Библиотека трендов.
 */

#ifndef TRENDS_H_
#define TRENDS_H_

#include "osc.h"
#include "errors/errors.h"
#include "future/future.h"
#include "q15.h"
#include <stdbool.h>
#include "fatfs/ff.h"


//! Число семплов трендов.
#define TRENDS_SAMPLES 4096

//! Число буферов трендов.
#define TRENDS_BUFFERS 4

//! Число трендов.
#define TRENDS_CHANNELS 16

//! Префикс имени файла.
//#define TRENDS_FILE_PREFIX_LEN 8

/**
 * Инициализирует тренды.
 * @return Код ошибки.
 */
extern err_t trends_init(void);

/**
 * Получает тренды.
 * @return Осциллограмма.
 */
extern osc_t* trends_get_osc(void);

/**
 * Добавляет текущие значения в тренды.
 */
extern void trends_append(void);

/**
 * Останавливает запись тренды в текуший буфер.
 * @param time Время до паузы.
 */
extern void trends_pause(iq15_t time);

/**
 * Получает флаг останова записи в текущий буфер.
 * @return Флаг останова записи.
 */
extern bool trends_paused(void);

/**
 * Возобновляет запись трендов в текущий буфер.
 * Сдвигает индекс буфера чтения на следующий буфер.
 */
extern void trends_resume(void);

/**
 * Получает время тренды.
 * @return Время тренды.
 */
extern iq15_t trends_time(void);

/**
 * Получает лимит тренда в одном файле.
 * @return Лимит.
 */
extern size_t trends_limit(void);

/**
 * Устанавливает лимит тренда в одном файле.
 * @param limit Лимит.
 */
extern void trends_set_limit(size_t limit);

/**
 * Получает время устаревания файлов.
 * @return Время устаревания.
 */
extern size_t trends_outdate(void);

/**
 * Устанавливает время устаревания файлов.
 * @param outdate Время устаревания файлов.
 */
extern void trends_set_outdate(size_t outdate);

/**
 * Получает интервал удаления устаревших файлов.
 * @return Интервал удаления устаревших файлов.
 */
extern size_t trends_outdate_interval(void);

/**
 * Устанавливает интервал удаления устаревших файлов.
 * @param interval Интервал удаления устаревших файлов.
 */
extern void trends_set_outdate_interval(size_t interval);

/**
 * Получает флаг разрешения записи трендров.
 * @return Флаг разрешения записи трендров.
 */
extern bool trends_enabled(void);

/**
 * Устанавливает флаг разрешения записи трендров.
 * @param enabled Флаг разрешения записи трендров.
 */
extern void trends_set_enabled(bool enabled);

/**
 * Сбрасывает тренд.
 */
extern void trends_reset(void);

/**
 * Начинает запись трендов.
 * @param future Будущее.
 * @return Код ошибки.
 */
extern err_t trends_start(future_t* future);

/**
 * Останавливает запись трендов.
 * @param future Будущее.
 * @return Код ошибки.
 */
extern err_t trends_stop(future_t* future);

/**
 * Синхронизирует тренды с файлом.
 * @param future Будущее.
 * @return Код ошибки.
 */
extern err_t trends_sync(future_t* future);

/**
 * Получает флаг записи трендов.
 * @return Флаг записи трендов.
 */
extern bool trends_running(void);

/**
 * Удаляет устаревшие файлы трендов.
 * @param dir_var Переменная папки для использования.
 * @param fno_var Переменная информации о файле для использования.
 * @return Код ошибки.
 */
extern err_t trends_remove_outdated(DIR* dir_var, FILINFO* fno_var);

#endif /* TRENDS_H_ */
