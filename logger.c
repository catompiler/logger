#include "logger.h"
#include "sdcard/sdcard.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tasks_conf.h"
#include <string.h>
#include "ain.h"
#include "din.h"
#include "dout.h"
#include "osc.h"
#include "trig.h"
#include "storage.h"
#include "oscs.h"
#include "utils/utils.h"
#include <stdio.h>
#include <time.h>

//! Размер буфера имени станции.
#define LOGGER_STATION_NAME_BUF_SIZE (LOGGER_STATION_NAME_MAX + 1)

//! Размер буфера идентификатора устройства.
#define LOGGER_DEV_ID_BUF_SIZE (LOGGER_DEV_ID_MAX + 1)


//! Задержка между попытками чтения конфигурации.
#define LOGGER_CONFIG_DELAY (pdMS_TO_TICKS(1000))

//! Задержка между попытками записи события.
#define LOGGER_EVENT_DELAY (pdMS_TO_TICKS(1000))

//! Время между итерациями проверок логгера.
#define LOGGER_ITER_DELAY_MS (1)
#define LOGGER_ITER_DELAY (pdMS_TO_TICKS(LOGGER_ITER_DELAY_MS))
#define LOGGER_ITER_DELAY_F (Q15(1.0 * LOGGER_ITER_DELAY_MS / 1000))

//! Размер очереди.
#define LOGGER_QUEUE_SIZE 4

//! Тип команды логгера.
typedef uint8_t logger_cmd_t;

//! Состояние чтения конфига.
typedef enum _Logger_Init_State {
    LOGGER_INIT_BEGIN = 0,
    LOGGER_INIT_WAIT_READ = 1,
    LOGGER_INIT_DONE = 2,
    LOGGER_INIT_RETRY = 3
} logger_init_state_t;

//! Состояние записи события.
typedef enum _Logger_Event_State {
    LOGGER_EVENT_BEGIN = 0,
    LOGGER_EVENT_WAIT_OSC = 1,
    LOGGER_EVENT_BEGIN_WRITE = 2,
    LOGGER_EVENT_WAIT_WRITE = 3,
    LOGGER_EVENT_DONE = 4,
    LOGGER_EVENT_RETRY = 5
} logger_event_state_t;

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
    char station_name[LOGGER_STATION_NAME_BUF_SIZE]; //!< Имя станции.
    char dev_id[LOGGER_DEV_ID_BUF_SIZE]; //!< Идентификатор устройства.
    logger_state_t state; //!< Состояние.
    q15_t osc_time_ratio; //!< Доля осциллограммы после события.
    // Конфиг.
    future_t conf_future; //!< Будущее.
    TickType_t conf_last_read; //!< Последнее чтение.
    logger_init_state_t init_state; //!< Состояние чтения.
    // Событие.
    event_t event; //!< Событие.
    TickType_t osc_wait_time; //!< Время ожидания осциллограммы.
    future_t event_future; //!< Будущее.
    TickType_t event_last_write; //!< Последняя запись.
    logger_event_state_t event_state; //!< Состояние записи.
} logger_t;

//! Логгер.
static logger_t logger;


static void logger_task_proc(void*);

err_t logger_init_task(void)
{
    logger.task_handle = xTaskCreateStatic(logger_task_proc, "logger_task",
                        LOGGER_STACK_SIZE, NULL, LOGGER_PRIORITY, logger.task_stack, &logger.task_buffer);

    if(logger.task_handle == NULL) return E_INVALID_VALUE;


    logger.queue_handle = xQueueCreateStatic(LOGGER_QUEUE_SIZE, sizeof(logger_cmd_t),
                                          (uint8_t*)logger.queue_storage, &logger.queue_buffer);

    if(logger.queue_handle == NULL) return E_INVALID_VALUE;

    return E_NO_ERROR;
}

err_t logger_init(void)
{
    memset(&logger, 0x0, sizeof(logger_t));

    err_t err = E_NO_ERROR;

    err = logger_init_task();
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

static void logger_go_init(void)
{
    logger.state = LOGGER_STATE_NOINIT;
    logger.init_state = LOGGER_INIT_BEGIN;
}

static void logger_go_run(void)
{
    logger.state = LOGGER_STATE_RUN;
}

static void logger_go_event(void)
{
    logger.state = LOGGER_STATE_EVENT;
    logger.event_state = LOGGER_EVENT_BEGIN;
}

static void logger_go_error(void)
{
    logger.state = LOGGER_STATE_ERROR;
}

static void logger_check_trigs(void)
{
	if(trig_check(LOGGER_ITER_DELAY_F)){
		if(logger.state == LOGGER_STATE_RUN){
		    logger_go_event();
		}
	}
}

static void logger_state_noinit(void)
{
	err_t err;

	switch(logger.init_state){
	case LOGGER_INIT_BEGIN:
	    ain_set_enabled(false);
	    trig_set_enabled(false);
        oscs_set_enabled(false);

	    ain_reset();
	    trig_reset();
        oscs_reset();

	    printf("Reading conf ini...");

	    // Сброс будущего.
        future_init(&logger.conf_future);

	    if(storage_read_conf(&logger.conf_future) == E_NO_ERROR){
	        printf("error!\r\n");
	        logger.init_state = LOGGER_INIT_WAIT_READ;
	    }
        break;

	case LOGGER_INIT_WAIT_READ:
	    if(future_done(&logger.conf_future)){
            // Чтение завершено.
            err = pvoid_to_int(err_t, future_result(&logger.conf_future));

            // Если чтение завершено успешно.
            if(err == E_NO_ERROR){

                printf("success!\r\n");

                logger.conf_last_read = 0;

                ain_set_enabled(true);
                trig_set_enabled(true);
                oscs_set_enabled(true);

                logger.init_state = LOGGER_INIT_DONE;
            }else{
                printf("fail!\r\n");

                // Ошибка доступа к SD-карте.
                if(err == E_IO_ERROR){
                    // Задержка попыток чтения.
                    logger.conf_last_read = xTaskGetTickCount();

                    logger.init_state = LOGGER_INIT_RETRY;
                }else{
                    logger_go_error();
                }
            }
        }
        break;
	case LOGGER_INIT_DONE:
	    logger_go_run();
        break;

	case LOGGER_INIT_RETRY:
        if(logger.conf_last_read == 0 ||
                ((xTaskGetTickCount() - logger.conf_last_read) >= LOGGER_CONFIG_DELAY)){

            logger.init_state = LOGGER_INIT_BEGIN;
        }
	    break;
	}
}

static void logger_state_run(void)
{
}

static void logger_state_event(void)
{
    size_t i;
    err_t err;
    iq15_t time_after;

    switch(logger.event_state){
    case LOGGER_EVENT_BEGIN:
        for(i = 0; i < TRIG_COUNT_MAX; i ++){
            if(trig_channel_activated(i)) break;
        }

        gettimeofday(&logger.event.time, NULL);
        logger.event.trig = i;

        time_after = oscs_time();
        time_after = iq15_mull(time_after, logger.osc_time_ratio);

        oscs_pause(time_after);

        logger.event_state = LOGGER_EVENT_WAIT_OSC;
        break;

    case LOGGER_EVENT_WAIT_OSC:
        if(oscs_paused()){
            logger.event_state = LOGGER_EVENT_BEGIN_WRITE;
        }
        break;

    case LOGGER_EVENT_BEGIN_WRITE:
        printf("Writing event...");

        // Сброс будущего.
        future_init(&logger.event_future);

        if(storage_write_event(&logger.event_future, &logger.event) == E_NO_ERROR){
            printf("error!\r\n");
            logger.event_state = LOGGER_EVENT_WAIT_WRITE;
        }
        break;

    case LOGGER_EVENT_WAIT_WRITE:
        if(future_done(&logger.event_future)){
            // Запись завершена.
            err = pvoid_to_int(err_t, future_result(&logger.event_future));

            // Если чтение завершено успешно.
            if(err == E_NO_ERROR){

                printf("success!\r\n");

                oscs_resume();

                logger.event_last_write = 0;

                logger.event_state = LOGGER_EVENT_DONE;
            }else{
                printf("fail!\r\n");

                // Ошибка доступа к SD-карте.
                if(err == E_IO_ERROR){
                    // Задержка попыток чтения.
                    logger.event_last_write = xTaskGetTickCount();

                    logger.event_state = LOGGER_EVENT_RETRY;
                }else{
                    logger_go_error();
                }
            }
        }
        break;
    case LOGGER_EVENT_DONE:
        logger_go_run();
        break;

    case LOGGER_EVENT_RETRY:
        if(logger.event_last_write == 0 ||
                ((xTaskGetTickCount() - logger.event_last_write) >= LOGGER_EVENT_DELAY)){

            logger.event_state = LOGGER_EVENT_BEGIN_WRITE;
        }
        break;
    }
}

static void logger_state_error(void)
{
}

static void logger_process_state(void)
{
	switch(logger.state){
	case LOGGER_STATE_NOINIT:
		logger_state_noinit();
		break;
	case LOGGER_STATE_RUN:
		logger_state_run();
		break;
	case LOGGER_STATE_EVENT:
		logger_state_event();
		break;
	case LOGGER_STATE_ERROR:
		logger_state_error();
		break;
	}
}

static void logger_process_cmd(logger_cmd_t* cmd)
{
}

static void logger_task_proc(void* arg)
{
    (void) arg;

    static logger_cmd_t cmd;

    logger_go_init();

    for(;;){
    	logger_check_trigs();
    	logger_process_state();

    	if(xQueueReceive(logger.queue_handle, &cmd, LOGGER_ITER_DELAY) == pdTRUE){
    		logger_process_cmd(&cmd);
		}
    }
}

logger_state_t logger_state(void)
{
	return logger.state;
}

void logger_set_osc_time_ratio(q15_t time)
{
    logger.osc_time_ratio = time;
}

err_t logger_set_station_name(const char* name)
{
    if(name == NULL) return E_NULL_POINTER;

    size_t len = strlen(name);

    if(len >= LOGGER_STATION_NAME_MAX) len = LOGGER_STATION_NAME_MAX;

    memcpy(logger.station_name, name, len);

    logger.station_name[len] = '\0';

    return E_NO_ERROR;
}

const char* logger_station_name(void)
{
    return logger.station_name;
}

err_t logger_set_dev_id(const char* dev_id)
{
    if(dev_id == NULL) return E_NULL_POINTER;

    size_t len = strlen(dev_id);

    if(len >= LOGGER_DEV_ID_MAX) len = LOGGER_DEV_ID_MAX;

    memcpy(logger.dev_id, dev_id, len);

    logger.dev_id[len] = '\0';

    return E_NO_ERROR;
}

const char* logger_dev_id(void)
{
    return logger.dev_id;
}
