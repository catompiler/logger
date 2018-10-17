#include "event.h"
#include "fatfs/ff.h"
#include <stdio.h>
#include "ain.h"
#include "din.h"
#include "osc.h"
#include "trig.h"
#include "q15.h"
#include "q15_str.h"


//! Записывать реальные значения.
#define EVENT_OSC_VALUE_ABSOLUTE 1

//! Записывать коэффициент перевода в абсолютное значнеие.
#define EVENT_OSC_WRITE_SCALE 1




//! Размер буфера записи.
#define EVENT_WRITE_BUF_SIZE 128
//! Буфер записи.
static char evbuf[EVENT_WRITE_BUF_SIZE];
//! Разделитель значений.
static const char* evdelim = ";";


//! Получает имя канала осциллограммы.
static const char* event_osc_channel_name(size_t osc_n)
{
    osc_src_t src = osc_channel_src(osc_n);
    size_t src_n = osc_channel_src_channel(osc_n);

    if(src == OSC_AIN) return ain_channel_name(src_n);
    else if(src == OSC_DIN) return din_name(src_n);

    return NULL;
}

//! Получает единицу измерения канала осциллограммы.
static const char* event_osc_channel_unit(size_t osc_n)
{
    osc_src_t src = osc_channel_src(osc_n);
    size_t src_n = osc_channel_src_channel(osc_n);

    if(src == OSC_AIN) return ain_channel_unit(src_n);

    return NULL;
}

#if EVENT_OSC_WRITE_SCALE == 1 || EVENT_OSC_VALUE_ABSOLUTE == 1
//! Получает коэффициент преобразования в реальное значение канала осциллограммы.
static iq15_t event_osc_channel_scale(size_t osc_n)
{
    osc_src_t src = osc_channel_src(osc_n);
    size_t src_n = osc_channel_src_channel(osc_n);

    if(src == OSC_AIN) return ain_channel_real_k(src_n);

    return IQ15I(1);
}
#endif

static err_t event_write_oscs_ch_names(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    const char* str = NULL;

    f_puts("Name", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){
        str = event_osc_channel_name(i);

        f_puts(evdelim, f);
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

static err_t event_write_oscs_ch_units(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    const char* str = NULL;

    f_puts("Unit", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){
        str = event_osc_channel_unit(i);

        f_puts(evdelim, f);
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
static err_t event_write_oscs_ch_scales(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    iq15_t scale = 0;

    f_puts("Scale", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){

        scale = event_osc_channel_scale(i);

        f_puts(evdelim, f);
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

static err_t event_write_oscs_ch_samples(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    size_t rate = 0;

    f_puts("Samples", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){

        rate = osc_channel_samples(i);

        f_puts(evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        f_printf(f, "%u", (unsigned int)rate);
        if(f_error(f)) return E_IO_ERROR;
    }
    f_puts("\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t event_write_oscs_ch_rates(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    size_t rate = 0;

    f_puts("Rate", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){

        rate = osc_channel_rate(i);

        f_puts(evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        f_printf(f, "%u", (unsigned int)rate);
        if(f_error(f)) return E_IO_ERROR;
    }
    f_puts("\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t event_write_oscs_ch_skews(FIL* f)
{
    size_t i;
    size_t used_count = osc_used_channels();

    size_t skew = 0;

    f_puts("Skew", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){

        skew = osc_channel_skew(i);

        f_puts(evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        f_printf(f, "%u", (unsigned int)skew);
        if(f_error(f)) return E_IO_ERROR;
    }
    f_puts("\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t event_write_oscs_chs_data(FIL* f, size_t index)
{
    size_t i;
    size_t used_count = osc_used_channels();

    size_t ch_rate;
    size_t ch_index;

    osc_value_t data;
#if EVENT_OSC_VALUE_ABSOLUTE == 1
    iq15_t scale;
#endif
    iq15_t value;

    f_puts("Data", f);
    if(f_error(f)) return E_IO_ERROR;

    for(i = 0; i < used_count; i ++){

        f_puts(evdelim, f);
        if(f_error(f)) return E_IO_ERROR;

        ch_rate = osc_channel_rate(i);

        if(ch_rate != 0){
            // Пропуск несуществующих значений.
            if((index % ch_rate) != 0) continue;

            ch_index = index / ch_rate;
        }else{
            ch_index = index;
        }

        // Пропуск если индекс больше количества.
        if(ch_index >= osc_channel_samples(i)) continue;

        ch_index = osc_channel_sample_index(i, ch_index);

        data = osc_channel_value(i, ch_index);

        if(osc_channel_src(i) == OSC_AIN){

#if EVENT_OSC_VALUE_ABSOLUTE == 1
            scale = event_osc_channel_scale(i);
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

static err_t event_write_oscs_chs_datas(FIL* f)
{
    err_t err = E_NO_ERROR;

    size_t i;
    size_t used_count = osc_used_channels();

    size_t max_samples = 0;
    size_t samples = 0;
    size_t ch_samples = 0;
    size_t ch_rate = 0;

    // Поиск канала с максимальным числом семплов.
    for(i = 0; i < used_count; i ++){
        ch_samples = osc_channel_samples(i);
        ch_rate = osc_channel_rate(i);

        samples = ch_samples * ch_rate;

        if(samples > max_samples){
            max_samples = samples;
        }
    }

    for(i = 0; i < max_samples; i ++){
        err = event_write_oscs_chs_data(f, i);
        if(err != E_NO_ERROR) return err;
    }

    return E_NO_ERROR;
}


static err_t event_write_oscs_impl(FIL* f)
{
    err_t err = E_NO_ERROR;

    size_t used_count = osc_used_channels();

    f_printf(f, "Channels: %u\n", (unsigned int)used_count);
    if(f_error(f)) return E_IO_ERROR;

    // Записать имена каналов.
    err = event_write_oscs_ch_names(f);
    if(err != E_NO_ERROR) return err;

    // Записать единиц измерения каналов.
    err = event_write_oscs_ch_units(f);
    if(err != E_NO_ERROR) return err;

#if EVENT_OSC_WRITE_SCALE == 1
    // Записать коэффициентов каналов.
    err = event_write_oscs_ch_scales(f);
    if(err != E_NO_ERROR) return err;
#endif

    // Записать число семплов каналов.
    err = event_write_oscs_ch_samples(f);
    if(err != E_NO_ERROR) return err;

    // Записать делителей частоты каналов.
    err = event_write_oscs_ch_rates(f);
    if(err != E_NO_ERROR) return err;

    // Записать разницу хода каналов.
    err = event_write_oscs_ch_skews(f);
    if(err != E_NO_ERROR) return err;

    // Записать разницу хода каналов.
    err = event_write_oscs_chs_datas(f);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

static err_t event_write_impl(FIL* f, struct tm* ev_tm, event_t* event)
{
    err_t err = E_NO_ERROR;

    f_printf(f, "Date%s%02d.%02d.%04d\n", evdelim,
                ev_tm->tm_mday, ev_tm->tm_mon, ev_tm->tm_year + 1900);
    if(f_error(f)) return E_IO_ERROR;

    f_printf(f, "Time%s%02d:%02d:%02d\n", evdelim,
                ev_tm->tm_hour, ev_tm->tm_min, ev_tm->tm_sec);
    if(f_error(f)) return E_IO_ERROR;

    const char* trig_name = trig_channel_name(event->trig);

    if(trig_name == NULL) trig_name = "";

    f_printf(f, "Trigger%s%u%s%s\n", evdelim, (unsigned int)event->trig,
                                     evdelim, trig_name);
    if(f_error(f)) return E_IO_ERROR;

    f_printf(f, "Freq%s%u\n", evdelim, AIN_SAMPLE_FREQ);
    if(f_error(f)) return E_IO_ERROR;

    err = event_write_oscs_impl(f);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

err_t event_write(event_t* event)
{
    if(event == NULL) return E_NULL_POINTER;

    err_t err = E_NO_ERROR;
    FIL f;
    FRESULT fr = FR_OK;
    struct tm* ev_tm = localtime(&event->time);

    if(ev_tm == NULL) return E_INVALID_VALUE;

    snprintf(evbuf, EVENT_WRITE_BUF_SIZE, "event_%02d.%02d.%04d_%02d-%02d-%02d.txt",
            ev_tm->tm_mday, ev_tm->tm_mon, ev_tm->tm_year + 1900,
            ev_tm->tm_hour, ev_tm->tm_min, ev_tm->tm_sec);

    fr = f_open(&f, evbuf, FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK) return E_IO_ERROR;

    err = event_write_impl(&f, ev_tm, event);

    f_close(&f);

    return err;
}


