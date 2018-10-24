#include "comtrade.h"
#include <string.h>
#include <time.h>
#include "q15_str.h"


//! Размер буфера.
#define COMTRADE_BUF_SIZE 32
//! Буфер.
static char ctrdbuf[COMTRADE_BUF_SIZE];



static err_t comtrade_cfg_write_station_line(FIL* f, comtrade_t* comtrade)
{
    const char* nullstr = "";

    const char* station_name = comtrade->station_name ? comtrade->station_name : nullstr;
    const char* rec_dev_id = comtrade->rec_dev_id ? comtrade->rec_dev_id : nullstr;

    f_printf(f, "%s,%s,%u\r\n", station_name, rec_dev_id, COMTRADE_STANDARD_YEAR);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_channels_num_line(FIL* f, comtrade_t* comtrade)
{
    size_t total_channels = comtrade->analog_channels + comtrade->digital_channels;

    f_printf(f, "%u,%uA,%uD\r\n", total_channels,
            comtrade->analog_channels, comtrade->digital_channels);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_analog_channel_line(FIL* f, comtrade_t* comtrade, size_t index)
{
    if(!comtrade->get_analog_channel) return E_NULL_POINTER;

    comtrade_analog_channel_t channel;

    memset(&channel, 0x0, sizeof(comtrade_analog_channel_t));

    comtrade->get_analog_channel(index, &channel);

    const char* nullstr = "";

    const char* ch_id = channel.ch_id ? channel.ch_id : nullstr;
    const char* ph = channel.ph ? channel.ph : nullstr;
    const char* ccbm = channel.ccbm ? channel.ccbm : nullstr;
    const char* uu = channel.uu ? channel.uu : nullstr;

    f_printf(f, "%u,%s,%s,%s,%s,", index + 1, ch_id, ph, ccbm, uu);
    if(f_error(f)) return E_IO_ERROR;

    if(iq15_tostr(ctrdbuf, COMTRADE_BUF_SIZE, channel.a) > 0){
        f_puts(ctrdbuf, f);
        if(f_error(f)) return E_IO_ERROR;
    }

    f_puts(",", f);
    if(f_error(f)) return E_IO_ERROR;

    if(iq15_tostr(ctrdbuf, COMTRADE_BUF_SIZE, channel.b) > 0){
        f_puts(ctrdbuf, f);
        if(f_error(f)) return E_IO_ERROR;
    }

    f_printf(f, ",%u,%d,%d,", channel.skew, channel.min, channel.max);
    if(f_error(f)) return E_IO_ERROR;

    if(iq15_tostr(ctrdbuf, COMTRADE_BUF_SIZE, channel.primary) > 0){
        f_puts(ctrdbuf, f);
        if(f_error(f)) return E_IO_ERROR;
    }

    f_puts(",", f);
    if(f_error(f)) return E_IO_ERROR;

    if(iq15_tostr(ctrdbuf, COMTRADE_BUF_SIZE, channel.secondary) > 0){
        f_puts(ctrdbuf, f);
        if(f_error(f)) return E_IO_ERROR;
    }

    f_printf(f, ",%c\r\n", channel.ps);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_digital_channel_line(FIL* f, comtrade_t* comtrade, size_t index)
{
    if(!comtrade->get_digital_channel) return E_NULL_POINTER;

    comtrade_digital_channel_t channel;

    memset(&channel, 0x0, sizeof(comtrade_digital_channel_t));

    comtrade->get_digital_channel(index, &channel);

    const char* nullstr = "";

    const char* ch_id = channel.ch_id ? channel.ch_id : nullstr;
    const char* ph = channel.ph ? channel.ph : nullstr;
    const char* ccbm = channel.ccbm ? channel.ccbm : nullstr;

    f_printf(f, "%u,%s,%s,%s,%u\r\n", index + 1, ch_id, ph, ccbm, channel.y);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_channels_lines(FIL* f, comtrade_t* comtrade)
{
    err_t err = E_NO_ERROR;

    size_t i;

    for(i = 0; i < comtrade->analog_channels; i ++){
        err = comtrade_cfg_write_analog_channel_line(f, comtrade, i);
        if(err != E_NO_ERROR) return err;
    }

    for(i = 0; i < comtrade->digital_channels; i ++){
        err = comtrade_cfg_write_digital_channel_line(f, comtrade, i);
        if(err != E_NO_ERROR) return err;
    }

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_line_freq_line(FIL* f, comtrade_t* comtrade)
{
    if(iq15_tostr(ctrdbuf, COMTRADE_BUF_SIZE, comtrade->lf) > 0){
        f_puts(ctrdbuf, f);
        if(f_error(f)) return E_IO_ERROR;
    }

    f_puts("\r\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_rate_line(FIL* f, comtrade_t* comtrade, size_t index)
{
    if(!comtrade->get_sample_rate) return E_NULL_POINTER;

    comtrade_sample_rate_t rate;

    memset(&rate, 0x0, sizeof(comtrade_sample_rate_t));

    comtrade->get_sample_rate(index, &rate);

    if(iq15_tostr(ctrdbuf, COMTRADE_BUF_SIZE, rate.samp) > 0){
        f_puts(ctrdbuf, f);
        if(f_error(f)) return E_IO_ERROR;
    }

    f_printf(f, ",%u\r\n", rate.endsamp);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_rate_lines(FIL* f, comtrade_t* comtrade)
{
    err_t err = E_NO_ERROR;
    size_t i;

    f_printf(f, "%u\r\n", comtrade->nrates);
    if(f_error(f)) return E_IO_ERROR;

    if(comtrade->nrates != 0){
        for(i = 0; i < comtrade->nrates; i ++){
            err = comtrade_cfg_write_rate_line(f, comtrade, i);
            if(err != E_NO_ERROR) return err;
        }
    }else{
        f_puts("0,9999999999\r\n", f);
        if(f_error(f)) return E_IO_ERROR;
    }

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_datetime(FIL* f, struct timeval* tv)
{
    struct tm* t = localtime(&tv->tv_sec);

    if(t == NULL) return E_INVALID_VALUE;

    suseconds_t usec = tv->tv_usec;
    if(usec > 999999) usec = 999999;

    if(t){
        f_printf(f, "%02u/%02u/%04u,%02u:%02u:%02u.%06u",
                t->tm_mday, t->tm_mon, t->tm_year + 1900,
                t->tm_hour, t->tm_min, t->tm_sec,
                usec);
        if(f_error(f)) return E_IO_ERROR;
    }

    f_puts("\r\n", f);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_datetimes(FIL* f, comtrade_t* comtrade)
{
    err_t err = E_NO_ERROR;

    err = comtrade_cfg_write_datetime(f, &comtrade->data_time);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_datetime(f, &comtrade->trigger_time);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_dat_file_type(FIL* f, comtrade_t* comtrade)
{
    (void) comtrade;

    f_printf(f, "%s\r\n", COMTRADE_DAT_FILE_TYPE);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

static err_t comtrade_cfg_write_timemult(FIL* f, comtrade_t* comtrade)
{
    f_printf(f, "%u\r\n", comtrade->timemult);
    if(f_error(f)) return E_IO_ERROR;

    return E_NO_ERROR;
}

err_t comtrade_write_cfg(FIL* f, comtrade_t* comtrade)
{
    if(f == NULL || comtrade == NULL) return E_NULL_POINTER;

    err_t err = E_NO_ERROR;

    err = comtrade_cfg_write_station_line(f, comtrade);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_channels_num_line(f, comtrade);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_channels_lines(f, comtrade);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_line_freq_line(f, comtrade);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_rate_lines(f, comtrade);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_datetimes(f, comtrade);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_dat_file_type(f, comtrade);
    if(err != E_NO_ERROR) return err;

    err = comtrade_cfg_write_timemult(f, comtrade);
    if(err != E_NO_ERROR) return err;

    return err;
}

err_t comtrade_append_dat(FIL* f, comtrade_t* comtrade, uint32_t sample_index, uint32_t timestamp)
{
    if(f == NULL || comtrade == NULL) return E_NULL_POINTER;
    if(comtrade->analog_channels != 0 && comtrade->get_analog_channel_value == NULL) return E_NULL_POINTER;
    if(comtrade->digital_channels != 0 && comtrade->get_digital_channel_value == NULL) return E_NULL_POINTER;

    UINT written;
    FRESULT fres = FR_OK;

    char* buf = &ctrdbuf[0];

    ((uint32_t*)buf)[0] = sample_index + 1;
    ((uint32_t*)buf)[1] = timestamp;

    fres = f_write(f, ctrdbuf, (sizeof(uint32_t) * 2), &written);
    if(fres != FR_OK || written != (sizeof(uint32_t) * 2)) return E_IO_ERROR;

    int16_t value;

    // analog channels.
    value = 0;
    size_t i;
    for(i = 0; i < comtrade->analog_channels; i ++){
        value = comtrade->get_analog_channel_value(i, sample_index);

        fres = f_write(f, &value, sizeof(int16_t), &written);
        if(fres != FR_OK || written != sizeof(int16_t)) return E_IO_ERROR;
    }

    // digital channels.
    value = 0;
    size_t bit = 0;
    for(i = 0; i < comtrade->digital_channels; i ++){
        if(comtrade->get_digital_channel_value(i, sample_index)){
            value |= (1 << bit);
        }

        bit ++;

        if(bit == (sizeof(int16_t) * 8)){
            fres = f_write(f, &value, sizeof(int16_t), &written);
            if(fres != FR_OK || written != sizeof(int16_t)) return E_IO_ERROR;

            bit = 0;
            value = 0;
        }
    }

    if(bit != 0){
        fres = f_write(f, &value, sizeof(int16_t), &written);
        if(fres != FR_OK || written != sizeof(int16_t)) return E_IO_ERROR;
    }

    return E_NO_ERROR;
}

