#include "storage.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tasks_conf.h"
#include <string.h>
#include "conf.h"
#include "utils/utils.h"
#include "fatfs/ff.h"


//! Размер очереди.
#define STORAGE_QUEUE_SIZE 4

//! Ожидание помещения в очередь.
#define STORAGE_QUEUE_DELAY portMAX_DELAY

//! Команда записи события.
typedef struct _Storage_Cmd_Write_Event {
    event_t event; //!< Событие.
} storage_cmd_wr_event_t;

//! Тип команды.
typedef struct _Storage_Cmd {
	uint8_t type; //!< Тип.
	future_t* future; //!< Будущее.
	union {
	    storage_cmd_wr_event_t write_event; //!< Запись события.
	};
} storage_cmd_t;

//! Команды.
//! Чтение конфига.
#define STORAGE_CMD_READ_CONF 0
//! Запись события.
#define STORAGE_CMD_WRITE_EVENT 1


//! Структура логгера.
typedef struct _Storage {
    // Задача.
    StackType_t task_stack[STORAGE_STACK_SIZE]; //!< Стэк задачи.
    StaticTask_t task_buffer; //!< Буфер задачи.
    TaskHandle_t task_handle; //!< Идентификатор задачи.
    // Очередь.
    storage_cmd_t queue_storage[STORAGE_QUEUE_SIZE]; //!< Данные очереди.
    StaticQueue_t queue_buffer; //!< Буфер очереди.
    QueueHandle_t queue_handle; //!< Идентификатор очереди.
    // Общий файл для всех задачь доступа к хранилищу.
    FIL file; //!< Общий файл.
} storage_t;

//! Логгер.
static storage_t storage;


static void storage_task_proc(void*);

static err_t storage_init_task(void)
{
    storage.task_handle = xTaskCreateStatic(storage_task_proc, "storage_task",
                        STORAGE_STACK_SIZE, NULL, STORAGE_PRIORITY, storage.task_stack, &storage.task_buffer);

    if(storage.task_handle == NULL) return E_INVALID_VALUE;

    storage.queue_handle = xQueueCreateStatic(STORAGE_QUEUE_SIZE, sizeof(storage_cmd_t),
                                          (uint8_t*)storage.queue_storage, &storage.queue_buffer);

    if(storage.queue_handle == NULL) return E_INVALID_VALUE;

    return E_NO_ERROR;
}

err_t storage_init(void)
{
    memset(&storage, 0x0, sizeof(storage_t));

    err_t err = E_NO_ERROR;

    err = storage_init_task();
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

static void storage_cmd_read_conf(storage_cmd_t* cmd)
{
	err_t err = E_NO_ERROR;

	memset(&storage.file, 0x0, sizeof(FIL));
	err = conf_read_ini(&storage.file);

	if(cmd->future) future_finish(cmd->future, int_to_pvoid(err));
}

static void storage_cmd_write_event(storage_cmd_t* cmd)
{
    err_t err = E_NO_ERROR;

    memset(&storage.file, 0x0, sizeof(FIL));
    err = event_write(&storage.file, &cmd->write_event.event);

    if(cmd->future) future_finish(cmd->future, int_to_pvoid(err));
}

static void storage_process_cmd(storage_cmd_t* cmd)
{
	switch(cmd->type){
	case STORAGE_CMD_READ_CONF:
		storage_cmd_read_conf(cmd);
		break;
	case STORAGE_CMD_WRITE_EVENT:
		storage_cmd_write_event(cmd);
		break;
	}
}

static void storage_task_proc(void* arg)
{
    (void) arg;

    static storage_cmd_t cmd;

	for(;;){
		if(xQueueReceive(storage.queue_handle, &cmd, portMAX_DELAY) == pdTRUE){
			storage_process_cmd(&cmd);
		}
	}
}

err_t storage_read_conf(future_t* future)
{
	storage_cmd_t cmd;

	cmd.type = STORAGE_CMD_READ_CONF;
	cmd.future = future;

	if(future){
		future_set_result(future, NULL);
		future_start(future);
	}

	if(xQueueSendToBack(storage.queue_handle, &cmd, STORAGE_QUEUE_DELAY) != pdTRUE){
		if(future) future_finish(future, int_to_pvoid(E_OUT_OF_MEMORY));
		return E_OUT_OF_MEMORY;
	}

	return E_NO_ERROR;
}

err_t storage_write_event(future_t* future, event_t* event)
{
    if(event == NULL) return E_NULL_POINTER;

	storage_cmd_t cmd;

	cmd.type = STORAGE_CMD_WRITE_EVENT;
	cmd.future = future;

	memcpy(&cmd.write_event.event, event, sizeof(event_t));

	if(future){
		future_set_result(future, NULL);
		future_start(future);
	}

	if(xQueueSendToBack(storage.queue_handle, &cmd, STORAGE_QUEUE_DELAY) != pdTRUE){
		if(future) future_finish(future, int_to_pvoid(E_OUT_OF_MEMORY));
		return E_OUT_OF_MEMORY;
	}

	return E_NO_ERROR;
}


