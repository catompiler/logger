/**
 * @file rootfs.h Корневая ФС.
 */

#ifndef ROOTFS_H_
#define ROOTFS_H_

#include "errors/errors.h"
#include "fatfs/diskio.h"
#include "fatfs/ff.h"
#include <stddef.h>



//! Тип функции инициализации диска.
typedef DSTATUS (*rootfs_disk_initialize_t)(void* disk);
//! Тип функции получения состояния диска.
typedef DSTATUS (*rootfs_disk_status_t)(void* disk);
//! Тип функции чтения диска.
typedef DRESULT (*rootfs_disk_read_t)(void* disk, BYTE* buff, DWORD sector, UINT count);
//! Тип функции записи диска.
typedef DRESULT (*rootfs_disk_write_t)(void* disk, const BYTE* buff, DWORD sector, UINT count);
//! Тип функции управления диском.
typedef DRESULT (*rootfs_disk_ioctl_t)(void* disk, BYTE cmd, void* buff);
//! Тип функции сброса состояния диска.
typedef DSTATUS (*rootfs_disk_reset_t)(void* disk);


//! Тип структуры ФС диска.
typedef struct _DiskFS {
	void* disk; //!< Пользовательские данные.
	FATFS* fatfs; //!< Объект файловой системы.
	rootfs_disk_initialize_t disk_initialize; //!< Инициализация диска.
	rootfs_disk_status_t disk_status; //!< Получение состояния диска.
	rootfs_disk_read_t disk_read; //!< Чтение диска.
	#if	_USE_WRITE
	rootfs_disk_write_t disk_write; //!< Запись диска.
	#endif
	#if	_USE_IOCTL
	rootfs_disk_ioctl_t disk_ioctl; //!< Управление диском.
	#endif
	rootfs_disk_reset_t disk_reset; //!< Сброс диска.
	size_t retries; //!< Число повторений чтения диска.
	size_t reinits; //!< Число переинициализаций диска после исчерпания повторных попыток.
} diskfs_t;


/**
 * Инициализирует корневую ФС.
 * @param disks Диски.
 * @param count Число дисков.
 * @return Код ошибки.
 */
extern err_t rootfs_init(diskfs_t* disks, size_t count);

/**
 * Инициализирует диск.
 * @param pdrv Номер диска.
 * @return Статус.
 */
DSTATUS rootfs_disk_initialize(BYTE pdrv);

/**
 * Получает статус диска.
 * @param pdrv Номер диска.
 * @return Статус.
 */
DSTATUS rootfs_disk_status(BYTE pdrv);

/**
 * Читает сектор диска.
 * @param pdrv Номер диска.
 * @param buff Буфер.
 * @param sector Сектор.
 * @param count Число секторов.
 * @return Результат.
 */
DRESULT rootfs_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);

#if _USE_WRITE
/**
 * Записывает сектор диска.
 * @param pdrv Номер диска.
 * @param buff Буфер.
 * @param sector Сектор.
 * @param count Число секторов.
 * @return Результат.
 */
DRESULT rootfs_disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
#endif // _USE_WRITE

#if _USE_IOCTL
/**
 * Управляет диском.
 * @param pdrv Номер диска.
 * @param cmd Команда.
 * @param buff Буфер.
 * @return Результат.
 */
DRESULT rootfs_disk_ioctl(BYTE pdrv, BYTE cmd, void* buff);
#endif // _USE_IOCTL


#ifdef USE_ROOTFS_FATFS_DISKIO

DSTATUS disk_initialize(BYTE pdrv)
{
    return rootfs_disk_initialize(pdrv);
}


DSTATUS disk_status(BYTE pdrv)
{
    return rootfs_disk_status(pdrv);
}


DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    return rootfs_disk_read(pdrv, buff, sector, count);
}

#if _USE_WRITE
DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    return rootfs_disk_write(pdrv, buff, sector, count);
}

#endif // _USE_WRITE

#if _USE_IOCTL
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    return rootfs_disk_ioctl(pdrv, cmd, buff);
}
#endif // _USE_IOCTL

#endif // USE_ROOTFS_FATFS_DISKIO


#endif /* ROOTFS_H_ */
