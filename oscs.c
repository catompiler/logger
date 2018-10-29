#include "oscs.h"

//! Структура осциллограмм.
typedef struct _Oscs {
    osc_value_t data[OSCS_SAMPLES]; //!< Данные осциллограмм.
    osc_buffer_t buffers[OSCS_BUFFERS]; //!< Буферы осциллограмм.
    osc_channel_t channels[OSCS_CHANNELS]; //!< Каналы осциллограмм.
    osc_t osc; //!< Осциллограмма.
} oscs_t;

//! Осциллограммы.
static oscs_t oscs;


err_t oscs_init(void)
{
    err_t err = E_NO_ERROR;

    err = osc_init(&oscs.osc, oscs.data, OSCS_SAMPLES,
                   oscs.buffers, OSCS_BUFFERS,
                   oscs.channels, OSCS_CHANNELS);
    if(err != E_NO_ERROR) return err;

    osc_set_buffer_mode(&oscs.osc, OSC_RING_IN_BUFFER);

    return E_NO_ERROR;
}

osc_t* oscs_get_osc(void)
{
    return &oscs.osc;
}

void oscs_append(void)
{
    osc_append(&oscs.osc);
}

void oscs_pause(iq15_t time)
{
    osc_pause(&oscs.osc, time);
}

bool oscs_paused(void)
{
    return osc_buffer_paused(&oscs.osc, osc_current_buffer(&oscs.osc));
}

void oscs_resume(void)
{
    osc_buffer_resume(&oscs.osc, osc_current_buffer(&oscs.osc));
    osc_next_buffer(&oscs.osc);
}

iq15_t oscs_time(void)
{
    return osc_time(&oscs.osc);
}

bool oscs_enabled(void)
{
    return osc_enabled(&oscs.osc);
}

void oscs_set_enabled(bool enabled)
{
    osc_set_enabled(&oscs.osc, enabled);
}

void oscs_reset(void)
{
    osc_reset(&oscs.osc);
}



