#include "trends.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "tasks_conf.h"
#include "logger.h"
#include "ain.h"
#include "din.h"
#include "comtrade.h"
#include "future/future.h"
#include "utils/utils.h"
#include "dsp/edge_detect.h"
#include <sys/time.h>
#include <time.h>
#include "fattime.h"
#include "storage.h"


//! Число попыток записи тренда.
#define TRENDS_WRITE_RETRIES 3

//! Размер очереди.
#define TRENDS_QUEUE_SIZE (TRENDS_BUFFERS * 2)

//! Ожидание помещения в очередь.
#define TRENDS_QUEUE_DELAY portMAX_DELAY

//! Период таймера.
#define TRENDS_TIMER_PERIOD_S 1
#define TRENDS_TIMER_PERIOD_MS (TRENDS_TIMER_PERIOD_S * 1000)
#define TRENDS_TIMER_PERIOD_TICKS (pdMS_TO_TICKS(TRENDS_TIMER_PERIOD_MS))

//! Размер буфера для записи файла.
#define TRENDS_BUF_SIZE 32

//! Размер имени файла.
#define TRENDS_FILENAME_LEN 32

//! Минимальное число семплов в файле.
#define TRENDS_LIMIT_SAMPLES_MIN 10

//! Безлимитное число семплов в файле.
#define TRENDS_LIMIT_SAMPLES_UNLIMIT 0

//! Тип команды.
typedef struct _Trends_Cmd {
    uint8_t type; //!< Тип.
    future_t* future; //!< Будущее.
} trends_cmd_t;

//! Команды.
//! Начало записи в файл.
#define TRENDS_CMD_START 0
//! Завершение записи в файл.
#define TRENDS_CMD_STOP 1
//! Запись буферов.
#define TRENDS_CMD_SYNC 2


//! Перечисление состояния трендов.
typedef enum _Trends_State {
    TRENDS_STATE_IDLE = 0,
    TRENDS_STATE_RUN = 1
} trends_state_t;

struct _Trends;

//! Структура данных осциллограммы комтрейд.
typedef struct _Trends_Osc_Data {
    struct _Trends* trends; //!< Тренды.
    osc_t* osc; //!< Осциллограмма.
    size_t buf; //!< Буфер.
    size_t start; //!< Начальный индекс.
    size_t count; //!< Количество.
} trends_osc_data_t;

//! Структура трендов.
typedef struct _Trends {
    // Задача.
    StackType_t task_stack[TRENDS_STACK_SIZE]; //!< Стэк задачи.
    StaticTask_t task_buffer; //!< Буфер задачи.
    TaskHandle_t task_handle; //!< Идентификатор задачи.
    // Очередь.
    trends_cmd_t queue_storage[TRENDS_QUEUE_SIZE]; //!< Данные очереди.
    StaticQueue_t queue_buffer; //!< Буфер очереди.
    QueueHandle_t queue_handle; //!< Идентификатор очереди.
    // Таймер.
    StaticTimer_t timer_buffer; //!< Буфер таймера.
    TimerHandle_t timer_handle; //!< Идентификатор таймера.
    // Данные.
    osc_value_t data[TRENDS_SAMPLES]; //!< Данные трендов.
    osc_buffer_t buffers[TRENDS_BUFFERS]; //!< Буферы трендов.
    osc_channel_t channels[TRENDS_CHANNELS]; //!< Каналы трендов.
    osc_t osc; //!< Осциллограмма.
    trends_state_t state; //!< Состояние.
    edge_detect_t ed_buf_fill; //!< Детектор момента заполнения буфера.
    bool need_sync; //!< Флаг необходимости синхронизации.
    size_t outdate; //!< Время устаревания файлов трендов в секундах.
    size_t outdate_interval; //!< Время удаления устаревших файлов в секундах.
    size_t limit; //!< Лимит в секундах.
    // Обмен с задачей.
    size_t limit_samples; //!< Лимит тренда в одном файле в семплах.
    // Данные задачи.
    FIL file; //!< Файл.
    comtrade_t comtrade; //!< Комтрейд.
    trends_osc_data_t osc_data; //!< Данные комтрейд.
    char file_base_name[TRENDS_FILENAME_LEN]; //!< Имя файла.
    size_t samples; //!< Число семплов в текущем тренде.
    size_t timestamp; //!< Отметка времени последнего семпла в тренде.
    //struct timeval data_time; //!< Время первых данных в файле.
    // Данные таймера.
    size_t outdate_counter; //!< Счётчик до удаления старых трендов.
} trends_t;

//! Тренды.
static trends_t trends;


static err_t trends_send_cmd_sync(future_t* future, TickType_t wait_ticks)
{
    trends_cmd_t cmd;
    cmd.type = TRENDS_CMD_SYNC;
    cmd.future = future;

    if(xQueueSendToBack(trends.queue_handle, &cmd, wait_ticks) != pdTRUE){
        return E_OUT_OF_MEMORY;
    }
    return E_NO_ERROR;
}

static err_t trends_send_cmd_start(future_t* future, TickType_t wait_ticks)
{
    trends_cmd_t cmd;
    cmd.type = TRENDS_CMD_START;
    cmd.future = future;

    if(xQueueSendToBack(trends.queue_handle, &cmd, wait_ticks) != pdTRUE){
        return E_OUT_OF_MEMORY;
    }
    return E_NO_ERROR;
}

static err_t trends_send_cmd_stop(future_t* future, TickType_t wait_ticks)
{
    trends_cmd_t cmd;
    cmd.type = TRENDS_CMD_STOP;
    cmd.future = future;

    if(xQueueSendToBack(trends.queue_handle, &cmd, wait_ticks) != pdTRUE){
        return E_OUT_OF_MEMORY;
    }
    return E_NO_ERROR;
}

static void trends_task_proc(void*);
static void trends_timer_proc(TimerHandle_t);
static err_t trends_init_task(void)
{
    trends.task_handle = xTaskCreateStatic(trends_task_proc, "trends_task",
                        TRENDS_STACK_SIZE, NULL, TRENDS_PRIORITY, trends.task_stack, &trends.task_buffer);

    if(trends.task_handle == NULL) return E_INVALID_VALUE;

    trends.queue_handle = xQueueCreateStatic(TRENDS_QUEUE_SIZE, sizeof(trends_cmd_t),
                                          (uint8_t*)trends.queue_storage, &trends.queue_buffer);

    if(trends.queue_handle == NULL) return E_INVALID_VALUE;

    vQueueAddToRegistry(trends.queue_handle, "trends_queue");

    trends.timer_handle = xTimerCreateStatic("trends_timer", TRENDS_TIMER_PERIOD_TICKS, 1, NULL,
                                            trends_timer_proc, &trends.timer_buffer);
    if(trends.timer_handle == NULL) return E_INVALID_VALUE;

    return E_NO_ERROR;
}

err_t trends_init(void)
{
    err_t err = E_NO_ERROR;

    memset(&trends, 0x0, sizeof(trends_t));

    err = trends_init_task();
    if(err != E_NO_ERROR) return err;

    err = osc_init(&trends.osc, trends.data, TRENDS_SAMPLES,
                   trends.buffers, TRENDS_BUFFERS,
                   trends.channels, TRENDS_CHANNELS);
    if(err != E_NO_ERROR) return err;

    osc_set_buffer_mode(&trends.osc, OSC_BUFFER_IN_RING);

    edge_detect_init(&trends.ed_buf_fill);

    return E_NO_ERROR;
}

osc_t* trends_get_osc(void)
{
    return &trends.osc;
}

void trends_append(void)
{
    if(trends.state != TRENDS_STATE_RUN){
        if(osc_pause_pending(&trends.osc)) osc_append(&trends.osc);
        return;
    }

    osc_append(&trends.osc);

    edge_detect_put(&trends.ed_buf_fill,
            osc_buffer_paused(&trends.osc, osc_current_buffer(&trends.osc)));

    if(edge_detect_state(&trends.ed_buf_fill) == EDGE_DETECT_RISING){
        trends.need_sync = true;
    }

    if(trends.need_sync){
        if(trends_send_cmd_sync(NULL, 0) == E_NO_ERROR){
            trends.need_sync = false;
        }
    }
}

void trends_pause(iq15_t time)
{
    osc_pause(&trends.osc, time);
}

bool trends_paused(void)
{
    return osc_buffer_paused(&trends.osc, osc_current_buffer(&trends.osc));
}

void trends_resume(void)
{
    osc_buffer_resume(&trends.osc, osc_current_buffer(&trends.osc));
    osc_next_buffer(&trends.osc);
}

iq15_t trends_time(void)
{
    return osc_time(&trends.osc);
}

size_t trends_limit(void)
{
    return trends.limit;
}

void trends_set_limit(size_t limit)
{
    trends.limit = limit;

    if(limit == TRENDS_LIMIT_SAMPLES_UNLIMIT){
        trends.limit_samples = TRENDS_LIMIT_SAMPLES_UNLIMIT;
        return;
    }

    iq15_t freq = osc_sample_freq(&trends.osc);
    lq15_t freq_samples = iq15_imull(freq, (int32_t)limit);

    size_t samples = (size_t)IQ15_INT(freq_samples);
    if(samples < TRENDS_LIMIT_SAMPLES_MIN) samples = TRENDS_LIMIT_SAMPLES_MIN;

    trends.limit_samples = samples;
}

size_t trends_outdate(void)
{
    return trends.outdate;
}

void trends_set_outdate(size_t outdate)
{
    trends.outdate = outdate;
}

size_t trends_outdate_interval(void)
{
    return trends.outdate_interval;
}

void trends_set_outdate_interval(size_t interval)
{
    trends.outdate_interval = interval;
}

bool trends_enabled(void)
{
    return osc_enabled(&trends.osc);
}

void trends_set_enabled(bool enabled)
{
    osc_set_enabled(&trends.osc, enabled);
}

void trends_reset(void)
{
    osc_reset(&trends.osc);
    edge_detect_reset(&trends.ed_buf_fill);
    trends.need_sync = false;
    trends.limit = 0;
    trends.limit_samples = 0;
    trends.outdate = 0;
    trends.outdate_interval = 0;
    trends.outdate_counter = 0;
}

err_t trends_start(future_t* future)
{
    err_t err = E_NO_ERROR;

    err = trends_send_cmd_start(future, TRENDS_QUEUE_DELAY);
    if(err != E_NO_ERROR) return err;

    if(xTimerStart(trends.timer_handle, TRENDS_QUEUE_DELAY) != pdPASS){
        return E_STATE;
    }

    trends.state = TRENDS_STATE_RUN;

    return E_NO_ERROR;
}

err_t trends_stop(future_t* future)
{
    err_t err = E_NO_ERROR;

    err = trends_send_cmd_stop(future, TRENDS_QUEUE_DELAY);
    if(err != E_NO_ERROR) return err;

    if(xTimerStop(trends.timer_handle, TRENDS_QUEUE_DELAY) != pdPASS){
        return E_STATE;
    }

    trends.state = TRENDS_STATE_IDLE;

    return E_NO_ERROR;
}

err_t trends_sync(future_t* future)
{
    err_t err = E_NO_ERROR;

    err = trends_send_cmd_sync(future, TRENDS_QUEUE_DELAY);
    if(err != E_NO_ERROR) return err;

    osc_pause_current(&trends.osc);

    return E_NO_ERROR;
}

bool trends_running(void)
{
    return trends.state == TRENDS_STATE_RUN;
}

/*#include "utils/critical.h"
static void trends_assert(bool value)
{
    if(!value){
        CRITICAL_ENTER();
        for(;;);
    }
}*/


/**
 * Получает данные об аналоговом канале.
 * @param index Индекс аналогового канала.
 * @param channel Данные о канале.
 */
static void comtrade_get_analog_channel(comtrade_t* comtrade, size_t index, comtrade_analog_channel_t* channel)
{
    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    osc_t* osc = osc_data->osc;

    size_t ch_index = osc_analog_channel_index(osc, index);
    if(ch_index == OSC_INDEX_INVALID) return;

    channel->ch_id = osc_channel_name(osc, ch_index);
    channel->ph = NULL;
    channel->ccbm = NULL;
    channel->uu = osc_channel_unit(osc, ch_index);
    channel->a = osc_channel_scale(osc, ch_index) / Q15_BASE;
    channel->b = IQ15(1);
    channel->skew = 0;
    channel->min = COMTRADE_DAT_MIN;
    channel->max = COMTRADE_DAT_MAX;
    channel->primary = IQ15(1);
    channel->secondary = IQ15(1);
    channel->ps = COMTRADE_PS_PRIMARY;
}

/**
 * Получает данные о цифровом канале.
 * @param index Индекс цифрового канала.
 * @param channel Данные о канале.
 */
static void comtrade_get_digital_channel(comtrade_t* comtrade, size_t index, comtrade_digital_channel_t* channel)
{
    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    osc_t* osc = osc_data->osc;

    size_t ch_index = osc_digital_channel_index(osc, index);
    if(ch_index == OSC_INDEX_INVALID) return;

    channel->ch_id = osc_channel_name(osc, ch_index);
    channel->ph = NULL;
    channel->ccbm = NULL;
    channel->y = false;
}

/**
 * Получает данные о частоте дискретизации.
 * @param index Индекс частоты дискретизации.
 * @param rate Данные о частоте дискретизации.
 */
static void comtrade_get_sample_rate(comtrade_t* comtrade, size_t index, comtrade_sample_rate_t* rate)
{
    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    trends_t* trends = osc_data->trends;
    osc_t* osc = osc_data->osc;
    size_t count = osc_data->count;

    (void) index;

    rate->samp = IQ15(AIN_SAMPLE_FREQ) / osc_rate(osc);
    rate->endsamp = trends->samples + count;
}

/**
 * Получает значение аналогового канала.
 * @param index Индекс канала.
 * @param sample Номер семпла канала.
 * @return Значение канала.
 */
static int16_t comtrade_get_analog_channel_value(comtrade_t* comtrade, size_t index, size_t sample)
{
    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    trends_t* trends = osc_data->trends;
    osc_t* osc = osc_data->osc;
    size_t buf = osc_data->buf;
    size_t start = osc_data->start;

    //trends_assert(index < osc_analog_channels(osc));
    //trends_assert(buf != OSC_INDEX_INVALID);

    if(buf == OSC_INDEX_INVALID) return COMTRADE_UNKNOWN_VALUE;

    size_t ch_index = osc_analog_channel_index(osc, index);
    if(ch_index == OSC_INDEX_INVALID) return COMTRADE_UNKNOWN_VALUE;

    size_t sample_index = sample - trends->samples + start;

    //if(index == 1) return (int16_t)(sample_index);

    osc_value_t value = osc_buffer_channel_value(osc, buf, ch_index,
                            osc_buffer_sample_number_index(osc, buf, sample_index));

    if(value == COMTRADE_UNKNOWN_VALUE) value = COMTRADE_DAT_MIN;

    return value;
}

/**
 * Получает значение цифрового канала.
 * @param index Индекс канала.
 * @param sample Номер семпла канала.
 * @return Значение канала.
 */
static bool comtrade_get_digital_channel_value(comtrade_t* comtrade, size_t index, size_t sample)
{
    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    trends_t* trends = osc_data->trends;
    osc_t* osc = osc_data->osc;
    size_t buf = osc_data->buf;
    size_t start = osc_data->start;

    //trends_assert(index < osc_analog_channels(osc));
    //trends_assert(buf != OSC_INDEX_INVALID);

    if(buf == OSC_INDEX_INVALID) return COMTRADE_UNKNOWN_VALUE;

    size_t ch_index = osc_digital_channel_index(osc, index);
    if(ch_index == OSC_INDEX_INVALID) return COMTRADE_UNKNOWN_VALUE;

    size_t sample_index = sample - trends->samples + start;

    osc_value_t value = osc_buffer_channel_value(osc, buf, ch_index,
                            osc_buffer_sample_number_index(osc, buf, sample_index));

    return value != 0;
}

static void trends_task_init_comtrade(void)
{
    comtrade_t* comtrade = &trends.comtrade;

    osc_t* osc = &trends.osc;

    struct timeval period_tv;

    osc_sample_period(osc, &period_tv);

    comtrade->station_name = logger_station_name();
    comtrade->rec_dev_id = logger_dev_id();
    comtrade->analog_channels = osc_analog_channels(osc);
    comtrade->get_analog_channel = comtrade_get_analog_channel;
    comtrade->digital_channels = osc_digital_channels(osc);
    comtrade->get_digital_channel = comtrade_get_digital_channel;
    comtrade->lf = IQ15(AIN_POWER_FREQ);
    comtrade->nrates = 1;
    comtrade->get_sample_rate = comtrade_get_sample_rate;

    comtrade->trigger_time.tv_sec = 0;
    comtrade->trigger_time.tv_usec = 0;

    comtrade->data_time.tv_sec = 0;
    comtrade->data_time.tv_usec = 0;

    comtrade->timemult = period_tv.tv_sec * 1000000 + period_tv.tv_usec;

    //comtrade.get_sample_timestamp = NULL;
    comtrade->get_analog_channel_value = comtrade_get_analog_channel_value;
    comtrade->get_digital_channel_value = comtrade_get_digital_channel_value;

    trends.osc_data.trends = &trends;
    trends.osc_data.osc = &trends.osc;
    trends.osc_data.buf = 0;
    trends.osc_data.start = 0;
    trends.osc_data.count = 0;

    comtrade->osc_data = (comtrade_osc_data_t)&trends.osc_data;
    comtrade->user_data = (void*)0;
}

static err_t trends_task_ctrd_write_cfg(comtrade_t* comtrade)
{
    err_t err = E_NO_ERROR;
    FRESULT fr = FR_OK;
    int res = 0;

    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    trends_t* trends = osc_data->trends;

    char filename[TRENDS_FILENAME_LEN];
    res = snprintf(filename, TRENDS_FILENAME_LEN, "%s.cfg", trends->file_base_name);
    if(res <= 0) return E_INVALID_VALUE;

    memset(&trends->file, 0x0, sizeof(FIL));
    fr = f_open(&trends->file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK) return E_IO_ERROR;

    err = comtrade_write_cfg(&trends->file, comtrade);

    f_close(&trends->file);

    return err;
}

static err_t trends_task_ctrd_write_dat(comtrade_t* comtrade)
{
    err_t err = E_NO_ERROR;
    FRESULT fr = FR_OK;
    int res = 0;

    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    trends_t* trends = osc_data->trends;

    char filename[TRENDS_FILENAME_LEN];
    res = snprintf(filename, TRENDS_FILENAME_LEN, "%s.dat", trends->file_base_name);
    if(res <= 0) return E_INVALID_VALUE;

    memset(&trends->file, 0x0, sizeof(FIL));
    fr = f_open(&trends->file, filename, FA_WRITE | FA_OPEN_ALWAYS); //FA_OPEN_APPEND
    if(fr != FR_OK) return E_IO_ERROR;

    size_t record_size = comtrade_dat_record_size(comtrade);
    size_t records_size = record_size * trends->samples;
    // Перейдём на нужную позицию в файле.
    fr = f_lseek(&trends->file, records_size);
    if(fr != FR_OK) return E_IO_ERROR;

    //size_t start = osc_data->start;
    size_t count = osc_data->count;
    size_t samples = trends->samples;
    size_t time = trends->timestamp;

    size_t timestamp;
    size_t nsample;
    size_t sample_index = 0;
    for(nsample = 0; nsample < count; nsample ++){

        sample_index = /*start +*/ nsample + samples;
        timestamp = nsample + time;

        err = comtrade_append_dat(&trends->file, comtrade, sample_index, timestamp);
        if(err != E_NO_ERROR) break;
    }

    f_close(&trends->file);

    return err;
}

static err_t trends_task_write_osc_buf_part(osc_t* osc, size_t buf, size_t start, size_t count)
{
    /*printf("samples %d write buf %d start %d count %d\r\n",
            (int)trends.samples, (int)buf, (int)start, (int)count);*/

    // Если число семплов для записи равно нулю - нечего записывать.
    if(count == 0) return E_NO_ERROR;

    err_t err = E_NO_ERROR;

    comtrade_t* comtrade = &trends.comtrade;

    // Для первого блока данных получим время.
    if(trends.samples == 0){
        struct timeval tv;

        err = osc_buffer_sample_time(osc, buf, start, &tv);
        if(err != E_NO_ERROR) return E_INVALID_VALUE;

        comtrade->trigger_time.tv_sec = tv.tv_sec;
        comtrade->trigger_time.tv_usec = tv.tv_usec;

        comtrade->data_time.tv_sec = tv.tv_sec;
        comtrade->data_time.tv_usec = tv.tv_usec;
    }

    trends_osc_data_t* osc_data = (trends_osc_data_t*)comtrade->osc_data;
    osc_data->buf = buf;
    osc_data->start = start;
    osc_data->count = count;

    int retry = 0;

    for(retry = 0; retry < TRENDS_WRITE_RETRIES; retry ++){
        err = trends_task_ctrd_write_cfg(comtrade);
        if(err == E_NO_ERROR) break;
    }
    if(err != E_NO_ERROR) return err;

    for(retry = 0; retry < TRENDS_WRITE_RETRIES; retry ++){
        err = trends_task_ctrd_write_dat(comtrade);
        if(err == E_NO_ERROR) break;
    }
    if(err != E_NO_ERROR) return err;

    trends.samples += count;

    return E_NO_ERROR;
}

static void trends_task_new_file(void);

static err_t trends_task_write_osc_buf(osc_t* osc, size_t buf)
{
    err_t err = E_NO_ERROR;
    size_t buf_count = osc_buffer_samples_count(osc, buf);

    size_t start = 0;
    size_t count = buf_count;

    if(trends.limit_samples != TRENDS_LIMIT_SAMPLES_UNLIMIT){

        if((trends.samples + buf_count) >= trends.limit_samples){
            if(trends.limit_samples >= trends.samples){
                start = 0;
                count = trends.limit_samples - trends.samples;

                err = trends_task_write_osc_buf_part(osc, buf, start, count);
                if(err != E_NO_ERROR){
                    trends.timestamp += buf_count;
                    return err;
                }

                trends.timestamp += count;

                start = count;
                count = buf_count - count;
            }

            trends_task_new_file();
        }
    }

    err = trends_task_write_osc_buf_part(osc, buf, start, count);

    trends.timestamp += count;

    return err;
}

static void trends_task_make_file_base_name(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm* t = localtime(&tv.tv_sec);

    if(t){
        snprintf(trends.file_base_name, TRENDS_FILENAME_LEN,
                "trend_%02d.%02d.%04d_%02d-%02d-%02d",
                t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
                t->tm_hour, t->tm_min, t->tm_sec);
    }else{
        snprintf(trends.file_base_name, TRENDS_FILENAME_LEN,
                "trend_%u", (unsigned int)tv.tv_sec);
    }
}

static void trends_task_reset_data(void)
{
    trends.samples = 0;
    memset(&trends.file_base_name, 0x0, TRENDS_FILENAME_LEN);
    memset(&trends.file, 0x0, sizeof(FIL));
    memset(&trends.comtrade, 0x0, sizeof(comtrade_t));
}

static void trends_task_new_file(void)
{
    trends_task_reset_data();
    trends_task_init_comtrade();
    trends_task_make_file_base_name();
}

static void trends_task_on_start(void)
{
    trends_task_new_file();
}

static void trends_task_on_stop(void)
{
}

static void trends_task_wait_if_pause(osc_t* osc, size_t buf)
{
    // Подождать ожидающейся паузы.
    for(;;){
        if(osc_buffer_paused(osc, buf)) break;
        if(!osc_pause_pending(osc)) break;
    }
}

static err_t trends_task_on_sync(void)
{
    osc_t* osc = &trends.osc;
    size_t buf;
    err_t err = E_NO_ERROR;
    err_t res_err = E_NO_ERROR;

    //printf("%u sync\r\n", (unsigned int)xTaskGetTickCount());

    buf = osc_current_buffer(osc);

    for(;;){
        trends_task_wait_if_pause(osc, buf);
        if(!osc_buffer_paused(osc, buf)) break;

        err = trends_task_write_osc_buf(osc, buf);
        if(err != E_NO_ERROR){
            printf("write buf error %d\r\n", (int)err);
            res_err = err;
        }

        osc_buffer_resume(osc, buf);
        buf = osc_next_buffer(osc);
    }

    return res_err;
}

static void trends_task_process_cmd_start(trends_cmd_t* cmd)
{
    err_t err = E_NO_ERROR;

    //printf("trends: start cmd\r\n");

    trends_task_on_start();

    if(cmd->future){
        future_finish(cmd->future, int_to_pvoid(err));
    }
}

static void trends_task_process_cmd_stop(trends_cmd_t* cmd)
{
    err_t err = E_NO_ERROR;

    //printf("trends: stop cmd\r\n");

    trends_task_on_stop();

    if(cmd->future){
        future_finish(cmd->future, int_to_pvoid(err));
    }
}

static void trends_task_process_cmd_sync(trends_cmd_t* cmd)
{
    err_t err = E_NO_ERROR;

    //printf("trends: sync cmd\r\n");

    err = trends_task_on_sync();

    if(cmd->future){
        future_finish(cmd->future, int_to_pvoid(err));
    }
}

static void trends_task_process_cmd(trends_cmd_t* cmd)
{
    switch(cmd->type){
    default:
        break;
    case TRENDS_CMD_START:
        trends_task_process_cmd_start(cmd);
        break;
    case TRENDS_CMD_STOP:
        trends_task_process_cmd_stop(cmd);
        break;
    case TRENDS_CMD_SYNC:
        trends_task_process_cmd_sync(cmd);
        break;
    }
}

static void trends_task_proc(void* arg)
{
    (void) arg;

    static trends_cmd_t cmd;

    trends_task_on_start();

    for(;;){
        if(xQueueReceive(trends.queue_handle, &cmd, portMAX_DELAY) == pdTRUE){
            trends_task_process_cmd(&cmd);
        }
    }
}

static void trends_timer_proc(TimerHandle_t xTimer)
{
    // Если разрешено одаление старых трендов.
    if(trends.outdate != 0){
        // Если подошёл период удаления.
        if(++ trends.outdate_counter >= trends.outdate_interval){
            // Если удалось запустить процесс удаления.
            if(storage_remove_outdated_trends() == E_NO_ERROR){
                // Сбросим счётчик.
                trends.outdate_counter = 0;
            }
        }
    }
}

err_t trends_remove_outdated(DIR* dir_var, FILINFO* fno_var)
{
    if(dir_var == NULL) return E_NULL_POINTER;
    if(fno_var == NULL) return E_NULL_POINTER;

    time_t cur_time = time(NULL);
    time_t file_time = 0;

    FRESULT fr;
    DIR* dir = dir_var;
    FILINFO* fno = fno_var;
    DWORD fdatetime = 0;

    if(trends.state != TRENDS_STATE_RUN) return E_NO_ERROR;

    fr = f_findfirst(dir, fno, "", "trend_*.*");

    while (fr == FR_OK && fno->fname[0]){
        //printf("%s\r\n", fno->fname);

        if(trends.state != TRENDS_STATE_RUN) break;

        fdatetime = ((DWORD)fno->fdate << 16) | fno->ftime;
        file_time = fattime_to_time(fdatetime, NULL);

        if((file_time + trends.outdate) <= cur_time){
            //printf("rm %s\r\n", fno->fname);

            f_unlink(fno->fname);
        }

        fr = f_findnext(dir, fno);
    }

    f_closedir(dir);

    return E_NO_ERROR;
}


