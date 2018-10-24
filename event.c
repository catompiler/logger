#include "event.h"
#include "fatfs/ff.h"
#include <stdio.h>
#include <string.h>
#include "logger.h"
#include "ain.h"
#include "din.h"
#include "osc.h"
#include "trig.h"
#include "q15.h"
#include "q15_str.h"
#include "comtrade.h"


/*
 * Общие данные.
 */

//! Максимальное число микросекунд для записи.
#define EVENT_USEC_MAX 999999

//! Размер буфера записи.
#define EVENT_WRITE_BUF_SIZE 32
//! Буфер записи.
static char evbuf[EVENT_WRITE_BUF_SIZE];

/*
 * Данные CSV.
 */

//! Записывать реальные значения.
#define EVENT_CSV_OSC_VALUE_ABSOLUTE 1

//! Записывать коэффициент перевода в абсолютное значнеие.
#define EVENT_CSV_OSC_WRITE_SCALE 1

//! Разделитель значений.
static const char* csv_evdelim = ";";

/*
 * Данные COMTRADE.
 */
//! Имя станции.
#define EVENT_CTRD_STATION_NAME ""
//! ID устройства.
#define EVENT_CTRD_DEV_ID ""
//! Год стандарта.
#define EVENT_CTRD_YEAR 1999


/*
 * Общие функции.
 */

//! Ограничивает микросекунды отметки времени.
ALWAYS_INLINE static suseconds_t event_osc_clamp_usec(suseconds_t usec)
{
    if(usec > EVENT_USEC_MAX) usec = EVENT_USEC_MAX;
    return usec;
}

/*
 * Функции для CSV.
 */

//! Записывает имена каналов.
static err_t event_csv_write_oscs_ch_names(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    const char* str = NULL;

    f_puts("Name", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){
        str = osc_channel_name(i);

        f_puts(csv_evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        if(str){
            f_puts(str, f);
            if(f_error(f)) return E_IO_ERROR;
        }
    }
    f_puts("\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

//! Записывает единицы измерения каналов.
static err_t event_csv_write_oscs_ch_units(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    const char* str = NULL;

    f_puts("Unit", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){
        str = osc_channel_unit(i);

        f_puts(csv_evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        if(str){
            f_puts(str, f);
            if(f_error(f)) return E_IO_ERROR;
        }
    }
    f_puts("\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

#if EVENT_OSC_WRITE_SCALE == 1
//! Записывает коэффициенты каналов.
static err_t event_csv_write_oscs_ch_scales(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    iq15_t scale = 0;

    f_puts("Scale", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){

        scale = osc_channel_scale(i);

        f_puts(csv_evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        if(iq15_tostr(evbuf, EVENT_WRITE_BUF_SIZE, scale) > 0){
            f_puts(evbuf, f);
            if(f_error(f)) return E_IO_ERROR;
        }
    }
    f_puts("\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}
#endif

/**
 * Записывает данные осциллограмм каналов.
 * @param f Файл.
 * @param index Индекс семпла.
 * @param time_tv Время семпла.
 * @return Код ошибки.
 */
static err_t event_csv_write_oscs_chs_data(FIL* f, size_t index, struct timeval* time_tv)
{
    size_t i;
    size_t ch_index = 0;
    size_t used_count = osc_used_channels();

    size_t sample_index = 0;

    osc_value_t data;
#if EVENT_OSC_VALUE_ABSOLUTE == 1
    iq15_t scale;
#endif
    iq15_t value;

    //f_puts("Data", f);
    //if(f_error(f)) return E_IO_ERROR;
    struct tm* end_tm = localtime(&time_tv->tv_sec);
    if(end_tm == NULL) return E_INVALID_VALUE;

    suseconds_t usec = event_osc_clamp_usec(time_tv->tv_usec);

    f_printf(f, "Data %02d/%02d/%04d,%02d:%02d:%02d.%06d", //evdelim,
                end_tm->tm_mday, end_tm->tm_mon, end_tm->tm_year + 1900,
                end_tm->tm_hour, end_tm->tm_min, end_tm->tm_sec, usec);
    if(f_error(f)) return E_IO_ERROR;

    // Для отладки продолжение вывода строк данных.
    if(index >= osc_samples()) return E_NO_ERROR;

    sample_index = osc_sample_index(index);

    for(i = 0; i < used_count; i ++){

        f_puts(csv_evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        ch_index = osc_used_index(i);

        data = osc_channel_value(ch_index, sample_index);

        if(osc_channel_src(ch_index) == OSC_AIN){

#if EVENT_OSC_VALUE_ABSOLUTE == 1
            scale = osc_channel_scale(ch_index);
            value = iq15_mull(data, scale);
#else
            value = data;
#endif

            if(iq15_tostr(evbuf, EVENT_WRITE_BUF_SIZE, value) > 0){
                f_puts(evbuf, f);
                if(f_error(f)) return E_IO_ERROR;
            }
        }else{//OSC_DIN
            f_printf(f, "%u", (unsigned int)data);
            if(f_error(f)) return E_IO_ERROR;
        }
    }
    f_puts("\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

//! Записывает данные осциллограмм.
static err_t event_csv_write_oscs_chs_datas(FIL* f)
{
    err_t err = E_NO_ERROR;

    size_t i;

    size_t max_samples = osc_samples();

    struct timeval time_tv;
    struct timeval period_tv;

    osc_start_time(&time_tv);
    osc_sample_period(&period_tv);

    for(i = 0; i < max_samples; i ++){
        err = event_csv_write_oscs_chs_data(f, i, &time_tv);
        if(err != E_NO_ERROR) return err;

        timeradd(&time_tv, &period_tv, &time_tv);
    }

    return E_NO_ERROR;
}

//! Записывает осциллограммы.
static err_t event_csv_write_oscs(FIL* f)
{
    err_t err = E_NO_ERROR;

    size_t used_count = osc_used_channels();

    f_printf(f, "Channels: %u\n", (unsigned int)used_count);
    if(f_error(f)) return E_IO_ERROR;

    // Записать имена каналов.
    err = event_csv_write_oscs_ch_names(f);
    if(err != E_NO_ERROR) return err;

    // Записать единиц измерения каналов.
    err = event_csv_write_oscs_ch_units(f);
    if(err != E_NO_ERROR) return err;

#if EVENT_OSC_WRITE_SCALE == 1
    // Записать коэффициентов каналов.
    err = event_csv_write_oscs_ch_scales(f);
    if(err != E_NO_ERROR) return err;
#endif

    // Записать разницу хода каналов.
    err = event_csv_write_oscs_chs_datas(f);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

/**
 * Записывает событие в CSV файл.
 * @param f Файл.
 * @param ev_tm Время события.
 * @param event Событие.
 * @return Код ошибки.
 */
static err_t event_csv_write_file(FIL* f, struct tm* ev_tm, event_t* event)
{
    err_t err = E_NO_ERROR;

    f_printf(f, "Date%s%02d.%02d.%04d\n", csv_evdelim,
                ev_tm->tm_mday, ev_tm->tm_mon, ev_tm->tm_year + 1900);
    if(f_error(f)) return E_IO_ERROR;

    suseconds_t usec = event_osc_clamp_usec(event->time.tv_usec);

    f_printf(f, "Time%s%02d:%02d:%02d.%06d\n", csv_evdelim,
                ev_tm->tm_hour, ev_tm->tm_min, ev_tm->tm_sec, usec);
    if(f_error(f)) return E_IO_ERROR;

    const char* trig_name = trig_channel_name(event->trig);

    if(trig_name == NULL) trig_name = "";

    f_printf(f, "Trigger%s%u%s%s\n", csv_evdelim, (unsigned int)event->trig,
                                     csv_evdelim, trig_name);
    if(f_error(f)) return E_IO_ERROR;

    f_printf(f, "Freq%s%u\n", csv_evdelim, AIN_SAMPLE_FREQ);
    if(f_error(f)) return E_IO_ERROR;

    f_printf(f, "Rate%s%u\n", csv_evdelim, osc_rate());
    if(f_error(f)) return E_IO_ERROR;

    f_printf(f, "Skew%s%u\n", csv_evdelim, osc_skew());
    if(f_error(f)) return E_IO_ERROR;

    f_printf(f, "Samples%s%u\n", csv_evdelim, osc_samples());
    if(f_error(f)) return E_IO_ERROR;

    err = event_csv_write_oscs(f);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

/**
 * Создаёт CSV файл и записывает в него событие.
 * @param event Событие.
 * @return Код ошибки.
 */
static err_t event_csv_write(event_t* event)
{
    if(event == NULL) return E_NULL_POINTER;

    err_t err = E_NO_ERROR;
    FIL f;
    FRESULT fr = FR_OK;
    struct tm* ev_tm = localtime(&event->time.tv_sec);

    if(ev_tm == NULL) return E_INVALID_VALUE;

    snprintf(evbuf, EVENT_WRITE_BUF_SIZE, "event_%02d.%02d.%04d_%02d-%02d-%02d.csv",
            ev_tm->tm_mday, ev_tm->tm_mon, ev_tm->tm_year + 1900,
            ev_tm->tm_hour, ev_tm->tm_min, ev_tm->tm_sec);

    fr = f_open(&f, evbuf, FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK) return E_IO_ERROR;

    err = event_csv_write_file(&f, ev_tm, event);

    f_close(&f);

    return err;
}

/**
 * Подсчитывает количество аналоговых и цифровых каналов.
 * @param analog_channels Число аналоговых каналов.
 * @param digital_channels Число цифровых каналов.
 */
static void event_ctrd_calc_channels(size_t* analog_channels, size_t* digital_channels)
{
    size_t i;
    size_t used_count = osc_used_channels();

    for(i = 0; i < used_count; i ++){
        if(osc_channel_type(i) == OSC_VAL){
            if(analog_channels) (*analog_channels) ++;
        }else{ // OSC_BIT
            if(digital_channels) (*digital_channels) ++;
        }
    }
}

/**
 * Получает данные об аналоговом канале.
 * @param index Индекс аналогового канала.
 * @param channel Данные о канале.
 */
static void comtrade_get_analog_channel(size_t index, comtrade_analog_channel_t* channel)
{
    size_t channel_n = 0;

    size_t i;
    size_t ch_index = 0;
    size_t used_count = osc_used_channels();

    for(i = 0; i < used_count; i ++){
        if(osc_channel_type(i) == OSC_VAL){
            if(channel_n == index){
                ch_index = osc_used_index(i);
                channel->ch_id = osc_channel_name(ch_index);
                channel->ph = NULL;
                channel->ccbm = NULL;
                channel->uu = osc_channel_unit(ch_index);
                channel->a = osc_channel_scale(ch_index) / Q15_BASE;
                channel->b = IQ15(1);
                channel->skew = 0;
                channel->min = COMTRADE_DAT_MIN;
                channel->max = COMTRADE_DAT_MAX;
                channel->primary = IQ15(1);
                channel->secondary = IQ15(1);
                channel->ps = COMTRADE_PS_PRIMARY;
                break;
            }
            channel_n ++;
        }
    }
}

/**
 * Получает данные о цифровом канале.
 * @param index Индекс цифрового канала.
 * @param channel Данные о канале.
 */
static void comtrade_get_digital_channel(size_t index, comtrade_digital_channel_t* channel)
{
    size_t channel_n = 0;

    size_t i;
    size_t ch_index = 0;
    size_t used_count = osc_used_channels();

    for(i = 0; i < used_count; i ++){
        if(osc_channel_type(i) == OSC_BIT){
            if(channel_n == index){
                ch_index = osc_used_index(i);
                channel->ch_id = osc_channel_name(ch_index);
                channel->ph = NULL;
                channel->ccbm = NULL;
                channel->y = false;
                break;
            }
            channel_n ++;
        }
    }
}

/**
 * Получает данные о частоте дискретизации.
 * @param index Индекс частоты дискретизации.
 * @param rate Данные о частоте дискретизации.
 */
static void comtrade_get_sample_rate(size_t index, comtrade_sample_rate_t* rate)
{
    (void) index;

    rate->samp = IQ15(AIN_SAMPLE_FREQ) / osc_rate();
    rate->endsamp = osc_samples();
}

/**
 * Получает значение аналогового канала.
 * @param index Индекс канала.
 * @param sample Номер семпла канала.
 * @return Значение канала.
 */
static int16_t comtrade_get_analog_channel_value(size_t index, size_t sample)
{
    size_t channel_n = 0;

    size_t i;
    size_t ch_index = 0;
    size_t used_count = osc_used_channels();

    for(i = 0; i < used_count; i ++){
        if(osc_channel_type(i) == OSC_VAL){
            if(channel_n == index){
                ch_index = osc_used_index(i);
                return osc_channel_value(ch_index, osc_sample_index(sample));
            }
            channel_n ++;
        }
    }
    return 0x8000;
}

/**
 * Получает значение цифрового канала.
 * @param index Индекс канала.
 * @param sample Номер семпла канала.
 * @return Значение канала.
 */
static bool comtrade_get_digital_channel_value(size_t index, size_t sample)
{
    size_t channel_n = 0;

    size_t i;
    size_t ch_index = 0;
    size_t used_count = osc_used_channels();

    for(i = 0; i < used_count; i ++){
        if(osc_channel_type(i) == OSC_BIT){
            if(channel_n == index){
                ch_index = osc_used_index(i);
                return osc_channel_value(ch_index, osc_sample_index(sample)) != 0;
            }
            channel_n ++;
        }
    }
    return false;
}

/**
 * Записывает событие в формат COMTRADE.
 * @param event Событие.
 * @param comtrade Структура COMTRADE.
 * @return Код ошибки.
 */
static err_t event_ctrd_write_cfg(event_t* event, comtrade_t* comtrade)
{
    err_t err = E_NO_ERROR;
    FIL f;
    FRESULT fr = FR_OK;
    struct tm* ev_tm = localtime(&event->time.tv_sec);
    if(ev_tm == NULL) return E_INVALID_VALUE;

    snprintf(evbuf, EVENT_WRITE_BUF_SIZE, "event_%02d.%02d.%04d_%02d-%02d-%02d.cfg",
            ev_tm->tm_mday, ev_tm->tm_mon, ev_tm->tm_year + 1900,
            ev_tm->tm_hour, ev_tm->tm_min, ev_tm->tm_sec);

    fr = f_open(&f, evbuf, FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK) return E_IO_ERROR;

    err = comtrade_write_cfg(&f, comtrade);

    f_close(&f);

    return err;
}

static err_t event_ctrd_write_dat(event_t* event, comtrade_t* comtrade)
{
    err_t err = E_NO_ERROR;
    FIL f;
    FRESULT fr = FR_OK;
    struct tm* ev_tm = localtime(&event->time.tv_sec);

    if(ev_tm == NULL) return E_INVALID_VALUE;

    snprintf(evbuf, EVENT_WRITE_BUF_SIZE, "event_%02d.%02d.%04d_%02d-%02d-%02d.dat",
            ev_tm->tm_mday, ev_tm->tm_mon, ev_tm->tm_year + 1900,
            ev_tm->tm_hour, ev_tm->tm_min, ev_tm->tm_sec);

    fr = f_open(&f, evbuf, FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK) return E_IO_ERROR;

    size_t nsample;
    for(nsample = 0; nsample < osc_samples(); nsample ++){
        err = comtrade_append_dat(&f, comtrade, nsample, nsample);
        if(err != E_NO_ERROR) break;
    }

    f_close(&f);

    return err;
}

err_t event_ctrd_write(event_t* event)
{
    if(event == NULL) return E_NULL_POINTER;

    err_t err = E_NO_ERROR;

    comtrade_t comtrade;
    memset(&comtrade, 0x0, sizeof(comtrade_t));

    size_t analog_channels = 0;
    size_t digital_channels = 0;

    event_ctrd_calc_channels(&analog_channels, &digital_channels);

    struct timeval time_tv;
    struct timeval period_tv;

    osc_start_time(&time_tv);
    osc_sample_period(&period_tv);

    comtrade.station_name = logger_station_name();
    comtrade.rec_dev_id = logger_dev_id();
    comtrade.analog_channels = analog_channels;
    comtrade.get_analog_channel = comtrade_get_analog_channel;
    comtrade.digital_channels = digital_channels;
    comtrade.get_digital_channel = comtrade_get_digital_channel;
    comtrade.lf = IQ15(AIN_POWER_FREQ);
    comtrade.nrates = 1;
    comtrade.get_sample_rate = comtrade_get_sample_rate;

    comtrade.trigger_time.tv_sec = event->time.tv_sec;
    comtrade.trigger_time.tv_usec = event->time.tv_usec;

    comtrade.data_time.tv_sec = time_tv.tv_sec;
    comtrade.data_time.tv_usec = time_tv.tv_usec;

    comtrade.timemult = period_tv.tv_sec * 1000000 + period_tv.tv_usec;

    //comtrade.get_sample_timestamp = NULL;
    comtrade.get_analog_channel_value = comtrade_get_analog_channel_value;
    comtrade.get_digital_channel_value = comtrade_get_digital_channel_value;

    err = E_NO_ERROR;

    err = event_ctrd_write_cfg(event, &comtrade);
    if(err != E_NO_ERROR) return err;

    err = event_ctrd_write_dat(event, &comtrade);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

err_t event_write(event_t* event)
{
    err_t err = E_NO_ERROR;

    /*err = event_csv_write(event);
    if(err != E_NO_ERROR) return err;*/

    err = event_ctrd_write(event);

    return err;
}
