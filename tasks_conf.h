/**
 * @file priorities.h Приоритеты задач.
 */

#ifndef TASKS_CONF_H_
#define TASKS_CONF_H_

#include "FreeRTOS.h"

// AIN.
#define AIN_PRIORITY 8
#define AIN_STACK_SIZE (configMINIMAL_STACK_SIZE)

// Logger.
#define LOGGER_PRIORITY 5
#define LOGGER_STACK_SIZE (configMINIMAL_STACK_SIZE)

#endif /* TASKS_CONF_H_ */
