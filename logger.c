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
#include "trends.h"
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
    LOGGER_INIT_START = 2,
    LOGGER_INIT_DONE = 3,
    LOGGER_INIT_RETRY = 4
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

//! Состояние останова записи.
typedef enum _Logger_Halt_State {
    LOGGER_HALT_BEGIN = 0,
    LOGGER_HALT_SYNC = 1,
    LOGGER_HALT_DONE = 2
} logger_halt_state_t;

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
    // Останов.
    future_t halt_future; //!< Будущее.
    logger_halt_state_t halt_state; //!< Состояние останова.
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
    if(logger.state == LOGGER_STATE_NOINIT) return;

    logger.state = LOGGER_STATE_NOINIT;
    logger.init_state = LOGGER_INIT_BEGIN;
}

static void logger_go_run(void)
{
    if(logger.state == LOGGER_STATE_RUN) return;

    logger.state = LOGGER_STATE_RUN;
}

static void logger_go_event(void)
{
    if(logger.state == LOGGER_STATE_EVENT) return;

    logger.state = LOGGER_STATE_EVENT;
    logger.event_state = LOGGER_EVENT_BEGIN;
}

static void logger_go_error(void)
{
    if(logger.state == LOGGER_STATE_ERROR) return;

    logger.state = LOGGER_STATE_ERROR;
}

static void logger_go_halt(void)
{
    if(logger.state == LOGGER_STATE_HALT) return;

    logger.state = LOGGER_STATE_HALT;
    logger.halt_state = LOGGER_HALT_BEGIN;
}

static void logger_check_dins(void)
{
    if(din_type_changed_state(DIN_RESET)) logger_go_init();
    if(din_type_changed_state(DIN_HALT)) logger_go_halt();
}

static void logger_update_douts(void)
{
    bool st_run = false;
    bool st_error = false;
    bool st_event = false;

    st_run |= logger.state == LOGGER_STATE_RUN;
    st_run |= logger.state == LOGGER_STATE_EVENT;
    st_run |= (logger.state == LOGGER_STATE_HALT && logger.halt_state != LOGGER_HALT_DONE);

    st_error |= logger.state == LOGGER_STATE_ERROR;

    st_event |= logger.state == LOGGER_STATE_EVENT;

    dout_set_type_state(DOUT_RUN, st_run);
    dout_set_type_state(DOUT_ERROR, st_error);
    dout_set_type_state(DOUT_EVENT, st_event);
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

	    // Остановить осциллограммы.
	    if(oscs_running()){
	        oscs_stop();
	    }

	    // Остановить тренды.
	    if(trends_running()){
	        if(trends_stop(NULL) != E_NO_ERROR) break;
	    }

	    // Запретить и сбросить АЦП.
	    ain_set_enabled(false);
	    ain_reset();

	    // Запретить и сбросить триггеры.
	    trig_set_enabled(false);
	    trig_reset();

	    // Запретить и сбросить осциллограммы.
        oscs_set_enabled(false);
        oscs_reset();

        // Запретить и сбросить тренды.
        trends_set_enabled(false);
        trends_reset();

	    printf("Reading conf ini...");

	    // Сброс будущего.
        future_init(&logger.conf_future);

	    if(storage_read_conf(&logger.conf_future) == E_NO_ERROR){
	        logger.init_state = LOGGER_INIT_WAIT_READ;
	    }else{
	        printf("error send cmd!\r\n");
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

                logger.init_state = LOGGER_INIT_START;
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

	case LOGGER_INIT_START:
        // Запустим осциллограммы.
        if(oscs_enabled() && !oscs_running()){
            oscs_start();
        }
        // Запустим тренды.
        if(trends_enabled() && !trends_running()){
            if(trends_start(NULL) != E_NO_ERROR){
                break;
            }
        }

        // Перейдём в состояние завершения.
        logger.init_state = LOGGER_INIT_DONE;
        break;

	case LOGGER_INIT_DONE:
	    logger_go_run();
        break;

	case LOGGER_INIT_RETRY:
	    // Если прошёл тайм-аут чтения.
        if(logger.conf_last_read == 0 ||
                ((xTaskGetTickCount() - logger.conf_last_read) >= LOGGER_CONFIG_DELAY)){

            // Запустим чтение снова.
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
    size_t trig_index;
    iq15_t time_after;

    switch(logger.event_state){
    case LOGGER_EVENT_BEGIN:

        if(!oscs_enabled()){
            logger.event_state = LOGGER_EVENT_DONE;
            break;
        }

        trig_index = 0;
        for(i = 0; i < TRIG_COUNT_MAX; i ++){
            if(trig_channel_activated(i)) trig_index = i;
        }

        gettimeofday(&logger.event.time, NULL);
        logger.event.trig = trig_index;

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

static void logger_state_halt(void)
{
    err_t err = E_NO_ERROR;

    switch(logger.halt_state){
    case LOGGER_HALT_BEGIN:

        if(!trends_enabled()){
            logger.halt_state = LOGGER_HALT_DONE;
            break;
        }

        if(trends_running()){
            if(trends_stop(NULL) != E_NO_ERROR) break;
        }

        printf("Sync trends...");

        // Сброс будущего.
        future_init(&logger.halt_future);

        err = trends_sync(&logger.halt_future);
        if(err == E_NO_ERROR){
            logger.halt_state = LOGGER_HALT_SYNC;
        }else{
            printf("error send cmd!\r\n");
        }
        break;
    case LOGGER_HALT_SYNC:
        if(future_done(&logger.halt_future)){
            // Синхронизация завершена.
            err = pvoid_to_int(err_t, future_result(&logger.halt_future));

            // Если чтение завершено успешно.
            if(err == E_NO_ERROR){
                printf("success!\r\n");

                logger.halt_state = LOGGER_HALT_DONE;
            }else{
                printf("fail!\r\n");

                logger_go_error();
            }
        }
        break;
    case LOGGER_HALT_DONE:
        break;
    }
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
    case LOGGER_STATE_HALT:
        logger_state_halt();
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

    for(;;){
        logger_check_dins();
    	logger_check_trigs();
    	logger_process_state();
    	logger_update_douts();

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
