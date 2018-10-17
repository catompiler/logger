#include "rootfs.h"
#include "fatfs/ff.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "defs/defs.h"


//! Длина пути диска.
#define ROOTFS_DISK_PATH_LEN 4


//! Тип корневой ФС.
typedef struct _RootFs {
	diskfs_t* diskfs; //!< Диски.
	size_t disks_count; //!< Количество дисков.
} rootfs_t;

//! Корневая ФС.
static rootfs_t rootfs;



//! Получает ФС диска по индексу.
ALWAYS_INLINE static diskfs_t* rootfs_diskfs(size_t n)
{
	return &rootfs.diskfs[n];
}

//! Создаёт путь диска по номеру.
static void rootfs_make_disk_path(char* path, size_t path_len, size_t n)
{
	snprintf(path, path_len, "%u:", n);
}

//! Монтирует диски.
static err_t rootfs_mount_disks(void)
{
	diskfs_t* diskfs = NULL;

	char disk_path[ROOTFS_DISK_PATH_LEN];

	size_t i;
	for(i = 0; i < rootfs.disks_count; i ++){
		diskfs = rootfs_diskfs(i);

		rootfs_make_disk_path(disk_path, ROOTFS_DISK_PATH_LEN, i);

		if(f_mount(diskfs->fatfs, disk_path, 0) != FR_OK) return E_INVALID_VALUE;
	}

	return E_NO_ERROR;
}

err_t rootfs_init(diskfs_t* disks, size_t count)
{
	if(disks == NULL) return E_NULL_POINTER;
	if(count == 0) return E_INVALID_VALUE;

	err_t err = E_NO_ERROR;

	memset(&rootfs, 0x0, sizeof(rootfs_t));

	rootfs.diskfs = disks;
	rootfs.disks_count = count;

	err = rootfs_mount_disks();
	if(err != E_NO_ERROR) return err;

	return E_NO_ERROR;
}

/*
 * FatFS diskio.
 */

/**
 * Получает диск по номеру диска.
 * @param pdrv Номер диска.
 * @return Диск.
 */
static diskfs_t* rootfs_get_diskfs(BYTE pdrv)
{
    if(pdrv >= rootfs.disks_count) return NULL;

    return &rootfs.diskfs[pdrv];
}

DSTATUS rootfs_disk_initialize(BYTE pdrv)
{
	diskfs_t* diskfs = rootfs_get_diskfs(pdrv);
    if(diskfs == NULL) return STA_NOINIT;

    if(diskfs->disk_initialize == NULL) return STA_NOINIT;

    return diskfs->disk_initialize(diskfs->disk);
}


DSTATUS rootfs_disk_status(BYTE pdrv)
{
	diskfs_t* diskfs = rootfs_get_diskfs(pdrv);
    if(diskfs == NULL) return STA_NOINIT;

    if(diskfs->disk_status == NULL) return STA_NOINIT;

    return diskfs->disk_status(diskfs->disk);
}


DRESULT rootfs_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
	diskfs_t* diskfs = rootfs_get_diskfs(pdrv);
    if(diskfs == NULL) return RES_PARERR;

    if(diskfs->disk_read == NULL) return STA_NOINIT;

    DRESULT res = RES_OK;
    size_t retries = diskfs->retries;
    size_t reinits = diskfs->reinits;

    for(;;){
        // Попытка операции с диском.
        res = diskfs->disk_read(diskfs->disk, buff, sector, count);
        // Успех - выход.
        if(res == RES_OK) break;
        // Ошибка параметра - нет смысла повторять.
        if(res == RES_PARERR) break;

        // Если можно повторить.
        if(retries){
            retries --;
            continue;
        }

        // Если можно переинициализировать.
        if(reinits && diskfs->disk_initialize){
            diskfs->disk_initialize(diskfs->disk);
            retries = diskfs->retries;
            reinits --;
            continue;
        }

        // Ошибка диска и ничего нельзя сделать.
        break;
    };

    return res;

	//return diskfs->disk_read(diskfs->disk, buff, sector, count);
}

#if _USE_WRITE
DRESULT rootfs_disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
	diskfs_t* diskfs = rootfs_get_diskfs(pdrv);
    if(diskfs == NULL) return RES_PARERR;

    if(diskfs->disk_write == NULL) return STA_NOINIT;

    DRESULT res = RES_OK;
    size_t retries = diskfs->retries;
    size_t reinits = diskfs->reinits;

    for(;;){
        // Попытка операции с диском.
        res = diskfs->disk_write(diskfs->disk, buff, sector, count);
        // Успех - выход.
        if(res == RES_OK) break;
        // Ошибка параметра - нет смысла повторять.
        if(res == RES_PARERR) break;

        // Если можно повторить.
        if(retries){
            retries --;
            continue;
        }

        // Если можно переинициализировать.
        if(reinits && diskfs->disk_initialize){
            diskfs->disk_initialize(diskfs->disk);
            retries = diskfs->retries;
            reinits --;
            continue;
        }

        // Ошибка диска и ничего нельзя сделать.
        break;
    };

    return res;

	//return diskfs->disk_write(diskfs->disk, buff, sector, count);
}

#endif // _USE_WRITE

#if _USE_IOCTL
DRESULT rootfs_disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
	diskfs_t* diskfs = rootfs_get_diskfs(pdrv);
    if(diskfs == NULL) return RES_PARERR;

    if(diskfs->disk_ioctl == NULL) return STA_NOINIT;

    DRESULT res = RES_OK;
    size_t retries = diskfs->retries;
    size_t reinits = diskfs->reinits;

    for(;;){
        // Попытка операции с диском.
        res = diskfs->disk_ioctl(diskfs->disk, cmd, buff);
        // Успех - выход.
        if(res == RES_OK) break;
        // Ошибка параметра - нет смысла повторять.
        if(res == RES_PARERR) break;

        // Если можно повторить.
        if(retries){
            retries --;
            continue;
        }

        // Если можно переинициализировать.
        if(reinits && diskfs->disk_initialize){
            diskfs->disk_initialize(diskfs->disk);
            retries = diskfs->retries;
            reinits --;
            continue;
        }

        // Ошибка диска и ничего нельзя сделать.
        break;
    };

    return res;

    //return diskfs->disk_ioctl(diskfs->disk, cmd, buff);
}
#endif // _USE_IOCTL

