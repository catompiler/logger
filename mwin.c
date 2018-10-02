#include "mwin.h"



err_t mwin_init(mwin_t* mwin, mwin_data_t* data, size_t size)
{
    if(data == NULL) return E_NULL_POINTER;
    if(size == 0) return E_INVALID_VALUE;

    mwin->data = data;
    mwin->size = size;
    mwin->sum = 0;
    mwin->index = 0;

    return E_NO_ERROR;
}

void mwin_reset(mwin_t* mwin)
{
    mwin->index = 0;
    mwin->sum = 0;
}

void mwin_put(mwin_t* mwin, mwin_data_t data)
{
    mwin_data_t old = mwin->data[mwin->index];
    mwin->data[mwin->index] = data;

    size_t next = mwin->index + 1;
    if(next >= mwin->size) next = 0;

    mwin->index = next;

    mwin->sum = iq15_sub(mwin->sum, old);
    mwin->sum = iq15_add(mwin->sum, data);
}
