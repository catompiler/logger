#include "fattime.h"
#include <stddef.h>
#include "bits/bits.h"


uint32_t fattime_from_time(const time_t* t)
{
    if(t == NULL) return 0;

    struct tm* lt = localtime(t);

    if(lt == NULL) return 0;

    DWORD ft = 0;

    // sec -> sec / 2.
    lt->tm_sec /= 2;
    // month 0..11 -> 1..12.
    if(lt->tm_mon < 12) lt->tm_mon += 1;
    // year 1900.. -> 1980...
    if(lt->tm_year > 80){
        lt->tm_year -= 80;
    }else{
        lt->tm_year = 0;
    }

    ft |= (lt->tm_sec)  << 0;
    ft |= (lt->tm_min)  << 5;
    ft |= (lt->tm_hour) << 11;
    ft |= (lt->tm_mday) << 16;
    ft |= (lt->tm_mon)  << 21;
    ft |= (lt->tm_year) << 25;

    return ft;
}

time_t fattime_to_time(uint32_t ft, time_t* t)
{
    struct tm ftm;

    ftm.tm_sec  = BIT_VALUE_LEN(ft, 0, 5);
    ftm.tm_min  = BIT_VALUE_LEN(ft, 5, 6);
    ftm.tm_hour = BIT_VALUE_LEN(ft, 11, 5);
    ftm.tm_mday = BIT_VALUE_LEN(ft, 16, 5);
    ftm.tm_mon  = BIT_VALUE_LEN(ft, 21, 4);
    ftm.tm_year = BIT_VALUE_LEN(ft, 25, 7);
    ftm.tm_isdst = 0;
    ftm.tm_wday = 0;
    ftm.tm_yday = 0;

    // sec / 2 -> sec.
    ftm.tm_sec *= 2;
    // month 1..12 -> 0..11.
    if(ftm.tm_mon > 0) ftm.tm_mon -= 1;
    // year 1980.. -> 1900...
    ftm.tm_year += 80;

    time_t tres = mktime(&ftm);

    if(t) *t = tres;

    return tres;
}
