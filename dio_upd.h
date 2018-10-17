/**
 * @file dio_upd.h Задача обновления цифровых входов / выходов.
 */

#ifndef DIO_UPD_H_
#define DIO_UPD_H_

#include "errors/errors.h"

/**
 * Инициализирует обновление цифровых входов/выходов.
 * @return Код ошибки.
 */
extern err_t dio_upd_init(void);

#endif /* DIO_UPD_H_ */
