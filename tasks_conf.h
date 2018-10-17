/**
 * @file priorities.h Приоритеты задач.
 */

#ifndef TASKS_CONF_H_
#define TASKS_CONF_H_

#include "FreeRTOS.h"

// AIN.
#define AIN_PRIORITY 8
#define AIN_STACK_SIZE (configMINIMAL_STACK_SIZE)

// Dio upd.
#define DIO_UPD_PRIORITY 6
#define DIO_UPD_STACK_SIZE (configMINIMAL_STACK_SIZE)

// Logger.
#define LOGGER_PRIORITY 5
#define LOGGER_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

// Storage.
#define STORAGE_PRIORITY 4
#define STORAGE_STACK_SIZE (configMINIMAL_STACK_SIZE * 4)

#endif /* TASKS_CONF_H_ */
