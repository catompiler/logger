/**
 * @file conf.h Реализация чтения конфигурации с SD карты.
 */

#ifndef CONF_H_
#define CONF_H_

#include "errors/errors.h"
#include <stdbool.h>


/**
 * Инициализирует конфиг.
 * @return Код ошибки.
 */
extern err_t conf_init(void);

/**
 * Читает конфигурацию из ini-файла.
 * @return Код ошибки.
 */
extern err_t conf_read_ini(void);


#endif /* CONF_H_ */
