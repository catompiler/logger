/**
 * @file logger.h Логгер.
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include "errors/errors.h"
#include "sdcard/sdcard.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


//! Перечисление состояний логгера.
typedef enum _Logger_State {
    LOGGER_STATE_NOINIT = 0, //!< Не инициализирован.
    LOGGER_STATE_RUN = 1, //!< Работа.
    LOGGER_STATE_EVENT = 2, //!< Обработка события.
    LOGGER_STATE_ERROR = 3 //!< Ошибка.
} logger_state_t;


extern err_t logger_init(sdcard_t* sdcard);

//extern err_t logger_process_

#endif /* LOGGER_H_ */
