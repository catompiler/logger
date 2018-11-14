/**
 * @file storage.h Библиотека чтения/записи с медленных носителей.
 */

#ifndef STORAGE_H_
#define STORAGE_H_

#include "errors/errors.h"
#include "future/future.h"
#include "event.h"


/**
 * Инициализирует хранилище.
 * @return Код ошибки.
 */
extern err_t storage_init(void);


/**
 * Читает файл конфигурации.
 * @param future Будущее. Может быть NULL.
 * @return Код ошибки.
 */
extern err_t storage_read_conf(future_t* future);


/**
 * Записывает событие.
 * @param future Будущее. Может быть NULL.
 * @return Код ошибки.
 */
extern err_t storage_write_event(future_t* future, event_t* event);

/**
 * Удаляет устаревшие файлы трендов.
 * @return Удаляет устаревшие файлы трендов.
 */
extern err_t storage_remove_outdated_trends(void);

#endif /* STORAGE_H_ */
