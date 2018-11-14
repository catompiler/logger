/**
 * @file fattime.h Функции преобразования времени FAT.
 */

#ifndef FATTIME_H_
#define FATTIME_H_

#include <stdint.h>
#include <time.h>
#include "fatfs/ff.h"


/**
 * Получает время FAT из времени UNIX.
 * @param t Время UNIX.
 * @return Время FAT.
 */
uint32_t fattime_from_time(const time_t* t);

/**
 * Получает время UNIX из времени FAT.
 * @param ft Время FAT.
 * @param t Время UNIX, может быть NULL.
 * @return Время UNIX.
 */
time_t fattime_to_time(uint32_t ft, time_t* t);


#endif /* FATTIME_H_ */
