#include "logger.h"
#include "sdcard/sdcard.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tasks_conf.h"
#include <string.h>


//! Размер очереди.
#define LOGGER_QUEUE_SIZE 4

//! Тип команды логгера.
typedef uint8_t logger_cmd_t;

//! Структура логгера.
typedef struct _Logger {
    // Задача.
    StackType_t task_stack[LOGGER_STACK_SIZE]; //!< Стэк задачи.
    StaticTask_t task_buffer; //!< Буфер задачи.
    TaskHandle_t task_handle; //!< Идентификатор задачи.
    // Очередь.
    logger_cmd_t queue_storage[LOGGER_QUEUE_SIZE]; //!< Данные очереди.
    StaticQueue_t queue_buffer; //!< Буфер очереди.
    QueueHandle_t queue_handle; //!< Идентификатор очереди.
    // Данные.
    sdcard_t* sdcard; //!< Карта памяти.
} logger_t;

//! Логгер.
static logger_t logger;


static void logger_task_proc(void*);

err_t logger_init_task(void)
{
    logger.task_handle = xTaskCreateStatic(logger_task_proc, "logger_task",
                        AIN_STACK_SIZE, NULL, AIN_PRIORITY, logger.task_stack, &logger.task_buffer);

    if(logger.task_handle == NULL) return E_INVALID_VALUE;


    logger.queue_handle = xQueueCreateStatic(LOGGER_QUEUE_SIZE, sizeof(logger_cmd_t),
                                          (uint8_t*)logger.queue_storage, &logger.queue_buffer);

    if(logger.queue_handle == NULL) return E_INVALID_VALUE;

    return E_NO_ERROR;
}

err_t logger_init(sdcard_t* sdcard)
{
    if(sdcard == NULL) return E_NULL_POINTER;

    memset(&logger, 0x0, sizeof(logger_t));

    err_t err = E_NO_ERROR;

    err = logger_init_task();
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

static void logger_task_proc(void* arg)
{
    (void) arg;

    static logger_cmd_t cmd;

    for(;;){
        if(xQueueReceive(logger.queue_handle, &cmd, portMAX_DELAY) == pdTRUE){
            // Process data & events.
        }
    }
}

