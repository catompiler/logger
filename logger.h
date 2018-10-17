/**
 * @file logger.h Логгер.
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include "errors/errors.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "q15.h"


//! Перечисление состояний логгера.
typedef enum _Logger_State {
    LOGGER_STATE_NOINIT = 0, //!< Не инициализирован.
    LOGGER_STATE_RUN = 1, //!< Работа.
    LOGGER_STATE_EVENT = 2, //!< Обработка события.
    LOGGER_STATE_ERROR = 3 //!< Ошибка.
} logger_state_t;


/**
 * Инициализирует логгер.
 * @return Код ошибки.
 */
extern err_t logger_init(void);

/**
 * Получает состояние логгера.
 * @return Состояние логгера.
 */
extern logger_state_t logger_state(void);

/**
 * Устанавливает долю времени осциллограммы после события.
 * @param time Доля времения осциллограммы после события.
 */
extern void logger_set_osc_after_event_time(q15_t time);

#endif /* LOGGER_H_ */
