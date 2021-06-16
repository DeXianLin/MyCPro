#ifndef __VL_TIME_HPP__
#define __VL_TIME_HPP__

#pragma once
#define HAVE_STRUCT_TIMESPEC

#include "vl_types.h"
#include "vl_const.h"
//#include <iostream>
//#include <windows.h>
#include <time.h>


int gettimeofday(struct timeval *tp, void *tzp);


class vl_time {
public:
    static vl_time currentTime();
    static void wait(const vl_time &delay);

    vl_time() {
        struct timeval tv;
        gettimeofday(&tv,0);
        sec = tv.tv_sec;
        microsec = tv.tv_usec;
    };
    vl_time(vl_uint32 seconds, vl_uint32 microseconds) :sec(seconds), microsec(microseconds) {}
    /* 获取时间戳 */
    vl_uint32 getSeconds() const {
        return sec;
    }
    vl_uint32 getMicroSeconds() const {
        return microsec;
    }

    vl_time &operator=(const vl_time &t);
    vl_time &operator-=(const vl_time &t);
    vl_time &operator+=(const vl_time &t);
    bool operator<(const vl_time &t) const;
    bool operator>(const vl_time &t) const;
    bool operator<=(const vl_time &t) const;
    bool operator>=(const vl_time &t) const;
private:
    vl_uint32 sec;
    vl_uint32 microsec;
};

inline vl_time vl_time::currentTime()
{
    struct timeval tv;

    gettimeofday(&tv,0);
    return vl_time((vl_uint32)tv.tv_sec,(vl_uint32)tv.tv_usec);
}

inline void vl_time::wait(const vl_time &delay)
{
    struct timespec req,rem;
    req.tv_sec = (time_t)delay.sec;
    req.tv_nsec = ((long)delay.microsec)*1000;
    nanosleep(&req,&rem);
//    Sleep(req.tv_sec);
    
    
}

inline vl_time &vl_time::operator-=(const vl_time &t)
{ 
    sec -= t.sec;
    if (t.microsec > microsec)
    {
        sec--;
        microsec += 1000000;
    }
    microsec -= t.microsec;
    return *this;
}

inline vl_time &vl_time::operator=(const vl_time &t) {
    this->sec = t.sec;
    this->microsec = t.microsec;
    return * this;
}

inline vl_time &vl_time::operator+=(const vl_time &t)
{ 
    sec += t.sec;
    microsec += t.microsec;
    if (microsec >= 1000000)
    {
        sec++;
        microsec -= 1000000;
    }
    return *this;
}

inline bool vl_time::operator<(const vl_time &t) const
{
    if (sec < t.sec)
        return true;
    if (sec > t.sec)
        return false;
    if (microsec < t.microsec)
        return true;
    return false;
}

inline bool vl_time::operator>(const vl_time &t) const
{
    if (sec > t.sec)
        return true;
    if (sec < t.sec)
        return false;
    if (microsec > t.microsec)
        return true;
    return false;
}

inline bool vl_time::operator<=(const vl_time &t) const
{
    if (sec < t.sec)
        return true;
    if (sec > t.sec)
        return false;
    if (microsec <= t.microsec)
        return true;
    return false;
}

inline bool vl_time::operator>=(const vl_time &t) const
{
    if (sec > t.sec)
        return true;
    if (sec < t.sec)
        return false;
    if (microsec >= t.microsec)
        return true;
    return false;
}

#endif
