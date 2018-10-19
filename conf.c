#include "conf.h"
#include <string.h>
#include <stdio.h>
#include "fatfs/ff.h"
#include "ini.h"
#include "ain.h"
#include "din.h"
#include "osc.h"
#include "trig.h"
#include "logger.h"
#include <sys/time.h>
#include <time.h>


//! Имя INI файла конфигурации.
static const char* config_ini = "config.ini";

//! Размер буфера для имени секции.
#define CONF_INI_SECT_BUF_LEN 16

//! Размер одной линии ini-файла.
#define INI_LINE_LEN 128

//! Тип конфигуратора.
typedef struct _Conf {
    ini_t ini; //!< Парсер ini.
    char ini_line[INI_LINE_LEN]; //!< Линии ini.
    FIL ini_file; //!< Файл.
} conf_t;

//! Конфигуратор.
static conf_t conf;


//! Функция чтения очередной линии файла.
static char* ini_get_line(char* line, int num, void* stream)
{
    return f_gets(line, num, (FIL*)stream);
}

//! Функция записи очередной линии файла.
static int ini_put_line(char* line, void* stream)
{
    return f_puts(line, (FIL*)stream);
}

//! Функция установки на начало файла.
static void ini_rewind(void* stream)
{
    f_lseek((FIL*)stream, 0);
}

err_t conf_init(void)
{
    memset(&conf, 0x0, sizeof(conf_t));

    err_t err = E_NO_ERROR;

    ini_init_t is;

    is.line = conf.ini_line;
    is.line_size = INI_LINE_LEN;
    is.get_line = ini_get_line;
    is.put_line = ini_put_line;
    is.rewind = ini_rewind;
    is.on_error = NULL;
    is.on_keyvalue = NULL;
    is.on_section = NULL;

    err = ini_init(&conf.ini, &is);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

static err_t conf_ini_read_time(ini_t* ini, FIL* f)
{
    int sec, min, hour, day, mon, year;

    char time_sect[CONF_INI_SECT_BUF_LEN];

    snprintf(time_sect, CONF_INI_SECT_BUF_LEN, "time");

    sec = ini_valuei(ini, time_sect, "sec", 0);
    if(f_error(f)) return E_IO_ERROR;

    min = ini_valuei(ini, time_sect, "min", 0);
    if(f_error(f)) return E_IO_ERROR;

    hour = ini_valuei(ini, time_sect, "hour", 0);
    if(f_error(f)) return E_IO_ERROR;

    day = ini_valuei(ini, time_sect, "day", 0);
    if(f_error(f)) return E_IO_ERROR;

    mon = ini_valuei(ini, time_sect, "mon", 0);
    if(f_error(f)) return E_IO_ERROR;

    year = ini_valuei(ini, time_sect, "year", 0);
    if(f_error(f)) return E_IO_ERROR;

    // UNIX epoch.
    if(year < 1970) year = 1970;
    // 1970+ -> 1900+.
    year -= 1900;
    // 1.. -> 0..
    if(mon) mon --;
    // 0.. -> 1..
    if(!day) day ++;

    struct tm lt;

    lt.tm_sec = sec;
    lt.tm_min = min;
    lt.tm_hour = hour;
    lt.tm_mday = day;
    lt.tm_mon = mon;
    lt.tm_year = year;
    lt.tm_isdst = 0;
    lt.tm_wday = 0;
    lt.tm_yday = 0;

    time_t t_conf = mktime(&lt);

    if(t_conf != (time_t)-1){

        time_t t_cur = time(NULL);

        if(t_cur < t_conf){

            struct timeval tv;
            tv.tv_sec = t_conf;
            tv.tv_usec = 0;

            settimeofday(&tv, NULL);
        }
    }

    return E_NO_ERROR;
}

static void conf_ini_store_str(char* dst, size_t size, const char* src)
{
    if(size == 0) return;
    if(src){
        while(*src && -- size){
            *dst ++ = *src ++;
        }
    }
    *dst = '\0';
}

static err_t conf_ini_read_logger(ini_t* ini, FIL* f)
{
    iq15_t time = 0;

    char log_sect[CONF_INI_SECT_BUF_LEN];

    snprintf(log_sect, CONF_INI_SECT_BUF_LEN, "log");

    time = ini_valuef(ini, log_sect, "osc_ratio", IQ15(0.5));
    if(f_error(f)) return E_IO_ERROR;

    time = iq15_sat(time);

    logger_set_osc_time_ratio(time);

    return E_NO_ERROR;
}

static err_t conf_ini_read_ains(ini_t* ini, FIL* f)
{
    ain_channel_type_t type;
    ain_channel_eff_type_t eff_type;
    uint16_t offset;
    iq15_t inst_gain;
    iq15_t eff_gain;
    iq15_t real_k;
    const char* str;
    char name[AIN_NAME_LEN + 1];
    char unit[AIN_UNIT_LEN + 1];
    bool enabled;

    char ain_sect[CONF_INI_SECT_BUF_LEN];

    size_t i;
    for(i = 0; i < AIN_CHANNELS_COUNT; i ++){
        snprintf(ain_sect, CONF_INI_SECT_BUF_LEN, "ain%u", i);

        type = ini_valuei(ini, ain_sect, "type", 0);
        if(f_error(f)) return E_IO_ERROR;

        eff_type = ini_valuei(ini, ain_sect, "eff_type", 0);
        if(f_error(f)) return E_IO_ERROR;

        offset = ini_valuei(ini, ain_sect, "offset", 0);
        if(f_error(f)) return E_IO_ERROR;

        inst_gain = ini_valuef(ini, ain_sect, "inst_gain", 0);
        if(f_error(f)) return E_IO_ERROR;

        eff_gain = ini_valuef(ini, ain_sect, "eff_gain", 0);
        if(f_error(f)) return E_IO_ERROR;

        real_k = ini_valuef(ini, ain_sect, "real_k", 0);
        if(f_error(f)) return E_IO_ERROR;

        str = ini_value(ini, ain_sect, "name", ain_sect);
        if(f_error(f)) return E_IO_ERROR;
        conf_ini_store_str(name, AIN_NAME_LEN + 1, str);

        str = ini_value(ini, ain_sect, "unit", NULL);
        if(f_error(f)) return E_IO_ERROR;
        conf_ini_store_str(unit, AIN_UNIT_LEN + 1, str);

        enabled = ini_valuei(ini, ain_sect, "enabled", 0);
        if(f_error(f)) return E_IO_ERROR;

        inst_gain = q15_sat(inst_gain);

        ain_init_channel(i, type, eff_type, offset, inst_gain, eff_gain);
        ain_channel_set_real_k(i, real_k);
        ain_channel_set_name(i, name);
        ain_channel_set_unit(i, unit);
        ain_channel_set_enabled(i, enabled);
    }

    return E_NO_ERROR;
}

static err_t conf_ini_read_dins(ini_t* ini, FIL* f)
{
    din_type_t type;
    iq15_t time;
    const char* str;
    char name[DIN_NAME_LEN];

    char din_sect[CONF_INI_SECT_BUF_LEN];

    size_t i;
    for(i = 0; i < DIN_COUNT; i ++){
        snprintf(din_sect, CONF_INI_SECT_BUF_LEN, "din%u", i);

        type = ini_valuei(ini, din_sect, "type", 0);
        if(f_error(f)) return E_IO_ERROR;

        time = ini_valuef(ini, din_sect, "time", 0);
        if(f_error(f)) return E_IO_ERROR;

        str = ini_value(ini, din_sect, "name", din_sect);
        if(f_error(f)) return E_IO_ERROR;
        conf_ini_store_str(name, DIN_NAME_LEN + 1, str);

        /*enabled = ini_valuei(ini, din_sect, "enabled", 0);
		if(f_error(f)) return E_IO_ERROR;*/

        time = q15_sat(time);

        din_channel_setup(i, type, time, name);
    }

    return E_NO_ERROR;
}

static err_t conf_ini_read_oscs(ini_t* ini, FIL* f)
{
    osc_src_t src;
    osc_type_t type;
    osc_src_type_t src_type;
    size_t src_channel;
    size_t rate;
    bool enabled;

    char osc_sect[CONF_INI_SECT_BUF_LEN];

    size_t i;
    for(i = 0; i < OSC_COUNT_MAX; i ++){
        snprintf(osc_sect, CONF_INI_SECT_BUF_LEN, "osc%u", i);

        src = ini_valuei(ini, osc_sect, "src", 0);
        if(f_error(f)) return E_IO_ERROR;

        type = ini_valuei(ini, osc_sect, "type", 0);
        if(f_error(f)) return E_IO_ERROR;

        src_type = ini_valuei(ini, osc_sect, "src_type", 0);
        if(f_error(f)) return E_IO_ERROR;

        src_channel = ini_valuei(ini, osc_sect, "src_channel", 0);
        if(f_error(f)) return E_IO_ERROR;

        enabled = ini_valuei(ini, osc_sect, "enabled", 0);
        if(f_error(f)) return E_IO_ERROR;

        osc_channel_init(i, src, type, src_type, src_channel);
        osc_channel_set_enabled(i, enabled);
    }

    rate = ini_valuei(ini, "osc", "rate", 1);
    if(f_error(f)) return E_IO_ERROR;

    return osc_init_channels(rate);
}

static err_t conf_ini_read_trigs(ini_t* ini, FIL* f)
{
    osc_src_t src;
    size_t src_channel;
    osc_src_type_t src_type;
    osc_type_t type;
    iq15_t time;
    iq15_t ref;
    const char* str;
    char name[TRIG_NAME_LEN + 1];
    bool enabled;

    trig_init_t init;

    char trig_sect[CONF_INI_SECT_BUF_LEN];

    size_t i;
    for(i = 0; i < TRIG_COUNT_MAX; i ++){
        snprintf(trig_sect, CONF_INI_SECT_BUF_LEN, "trig%u", i);

        src = ini_valuei(ini, trig_sect, "src", 0);
        if(f_error(f)) return E_IO_ERROR;

        src_channel = ini_valuei(ini, trig_sect, "src_channel", 0);
        if(f_error(f)) return E_IO_ERROR;

        src_type = ini_valuei(ini, trig_sect, "src_type", 0);
        if(f_error(f)) return E_IO_ERROR;

        type = ini_valuei(ini, trig_sect, "type", 0);
        if(f_error(f)) return E_IO_ERROR;

        time = ini_valuef(ini, trig_sect, "time", 0);
        if(f_error(f)) return E_IO_ERROR;

        ref = ini_valuef(ini, trig_sect, "ref", 0);
        if(f_error(f)) return E_IO_ERROR;

        str = ini_value(ini, trig_sect, "name", NULL);
        if(f_error(f)) return E_IO_ERROR;
        conf_ini_store_str(name, TRIG_NAME_LEN + 1, str);

        enabled = ini_valuei(ini, trig_sect, "enabled", 0);
        if(f_error(f)) return E_IO_ERROR;

        time = q15_sat(time);
        ref = q15_sat(ref);

        init.src = src;
        init.src_channel = src_channel;
        init.src_type = src_type;
        init.type = type;
        init.time = time;
        init.ref = ref;
        init.name = name;

        trig_channel_init(i, &init);
        trig_channel_set_enabled(i, enabled);
    }

    return E_NO_ERROR;
}

static err_t conf_read_ini_impl(FIL* f)
{
    err_t err = E_NO_ERROR;

    ini_set_stream(&conf.ini, (void*)f);

    err = conf_ini_read_time(&conf.ini, f);
    if(err != E_NO_ERROR) return err;

    err = conf_ini_read_logger(&conf.ini, f);
    if(err != E_NO_ERROR) return err;

    err = conf_ini_read_ains(&conf.ini, f);
    if(err != E_NO_ERROR) return err;

    err = conf_ini_read_dins(&conf.ini, f);
    if(err != E_NO_ERROR) return err;

    err = conf_ini_read_oscs(&conf.ini, f);
    if(err != E_NO_ERROR) return err;

    err = conf_ini_read_trigs(&conf.ini, f);
    if(err != E_NO_ERROR) return err;

    return E_NO_ERROR;
}

err_t conf_read_ini(void)
{
    FRESULT fr = FR_OK;
    err_t err = E_NO_ERROR;

    fr = f_open(&conf.ini_file, config_ini, FA_READ);
    if(fr != FR_OK) return E_IO_ERROR;

    err = conf_read_ini_impl(&conf.ini_file);

    f_close(&conf.ini_file);

    return err;
}

