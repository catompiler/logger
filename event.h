/**
 * @file event.h Запись событий.
 */

#ifndef EVENT_H_
#define EVENT_H_

#include "errors/errors.h"
#include <sys/time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fatfs/ff.h"


//! Структура события.
typedef struct _Event {
    struct timeval time; //!< Время события.
    size_t trig; //!< Номер триггера.
} event_t;

/**
 * Записывает событие в файл.
 * @param filevar Переменная-файл для использования.
 * @param event Событие.
 * @return Код ошибки.
 */
extern err_t event_write(FIL* filevar, event_t* event);


#endif /* EVENT_H_ */
