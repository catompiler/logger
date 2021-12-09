/**
 * @file logger.h Логгер.
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include "errors/errors.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "q15/q15.h"


//! Длина имени станции.
#define LOGGER_STATION_NAME_MAX 32

//! Длина идентификатора устройства.
#define LOGGER_DEV_ID_MAX 32


//! Перечисление состояний логгера.
typedef enum _Logger_State {
    LOGGER_STATE_NOINIT = 0, //!< Не инициализирован.
    LOGGER_STATE_RUN = 1, //!< Работа.
    LOGGER_STATE_EVENT = 2, //!< Обработка события.
    LOGGER_STATE_ERROR = 3, //!< Ошибка.
    LOGGER_STATE_HALT = 4 //!< Останов.
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
extern void logger_set_osc_time_ratio(q15_t time);

/**
 * Устанавливает имя станции.
 * @param name Имя станции.
 * @return Код ошибки.
 */
extern err_t logger_set_station_name(const char* name);

/**
 * Получает имя станции.
 * @return Имя станции.
 */
extern const char* logger_station_name(void);

/**
 * Устанавливает идентификатор устройства.
 * @param name Идентификатор устройства.
 * @return Код ошибки.
 */
extern err_t logger_set_dev_id(const char* dev_id);

/**
 * Получает идентификатор устройства.
 * @return Идентификатор устройства.
 */
extern const char* logger_dev_id(void);

#endif /* LOGGER_H_ */
