/**
 * @file oscs.h Вспомогательная прослойка осциллограмм.
 */

#ifndef OSCS_H_
#define OSCS_H_

#include "osc.h"
#include "errors/errors.h"
#include "q15.h"
#include <stdbool.h>

//! Число семплов осциллограмм.
#define OSCS_SAMPLES 1024

//! Число буферов осциллограмм.
#define OSCS_BUFFERS 2

//! Число осциллограмм.
#define OSCS_CHANNELS 16

/**
 * Инициализирует осциллограммы.
 * @return Код ошибки.
 */
extern err_t oscs_init(void);

/**
 * Получает осциллограммы.
 * @return Осциллограмма.
 */
extern osc_t* oscs_get_osc(void);

/**
 * Добавляет текущие значения в осциллограмму.
 */
extern void oscs_append(void);

/**
 * Останавливает запись осциллограммы в текуший буфер.
 * @param time Время до паузы.
 */
extern void oscs_pause(iq15_t time);

/**
 * Получает флаг останова записи в текущий буфер.
 * @return Флаг останова записи.
 */
extern bool oscs_paused(void);

/**
 * Возобновляет запись осциллограмм в текущий буфер.
 * Сдвигает индекс буфера чтения на следующий буфер..
 */
extern void oscs_resume(void);

/**
 * Получает время осциллограммы.
 * @return Время осциллограммы.
 */
extern iq15_t oscs_time(void);

/**
 * Получает флаг разрешения записи осциллограмм.
 * @return Флаг разрешения записи осциллограмм.
 */
extern bool oscs_enabled(void);

/**
 * Устанавливает флаг разрешения записи осциллограмм.
 * @param enabled Флаг разрешения записи осциллограмм.
 */
extern void oscs_set_enabled(bool enabled);

/**
 * Сбрасывает осциллограмму.
 */
extern void oscs_reset(void);

/**
 * Запускает запись осциллограмм.
 */
extern void oscs_start(void);

/**
 * Останавливает запись осциллограмм.
 */
extern void oscs_stop(void);

/**
 * Получает флаг записи осциллограмм.
 * @return Флаг записи осциллограмм.
 */
extern bool oscs_running(void);

#endif /* OSCS_H_ */
