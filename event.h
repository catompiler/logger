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


//! Структура события.
typedef struct _Event {
    struct timeval time; //!< Время события.
    struct timeval end_time; //!< Время конца осциллограммы.
    size_t trig; //!< Номер триггера.
} event_t;


extern err_t event_write(event_t* event);


#endif /* EVENT_H_ */
