#include "dio_upd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tasks_conf.h"
#include <string.h>
#include "din.h"
#include "dout.h"

//! Время между обновлениями входов/выходов.
#define DIO_UPD_ITER_TIME_MS (10)
#define DIO_UPD_ITER_TIME_TICKS (pdMS_TO_TICKS(DIO_UPD_ITER_TIME_MS))
#define DIO_UPD_ITER_TIME_F (Q15(1.0 * DIO_UPD_ITER_TIME_MS / 1000))

//! Структура логгера.
typedef struct _Dio_Upd {
    // Задача.
    StackType_t task_stack[DIO_UPD_STACK_SIZE]; //!< Стэк задачи.
    StaticTask_t task_buffer; //!< Буфер задачи.
    TaskHandle_t task_handle; //!< Идентификатор задачи.
} dio_upd_t;

//! Логгер.
static dio_upd_t dio_upd;


static void dio_upd_task_proc(void*);

err_t dio_upd_init_task(void)
{
    dio_upd.task_handle = xTaskCreateStatic(dio_upd_task_proc, "dio_upd_task",
                        DIO_UPD_STACK_SIZE, NULL, DIO_UPD_PRIORITY, dio_upd.task_stack, &dio_upd.task_buffer);

    if(dio_upd.task_handle == NULL) return E_INVALID_VALUE;

    return E_NO_ERROR;
}

err_t dio_upd_init(void)
{
    memset(&dio_upd, 0x0, sizeof(dio_upd_t));

    err_t err = E_NO_ERROR;

    err = dio_upd_init_task();
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

static void dio_upd_task_proc(void* arg)
{
    (void) arg;

    for(;;){
    	din_process(DIO_UPD_ITER_TIME_F);
		dout_process();

    	vTaskDelay(DIO_UPD_ITER_TIME_TICKS);
    }
}



