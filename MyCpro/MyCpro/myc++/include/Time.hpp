#ifndef __TIME_HPP__
#define __TIME_HPP__

//#include <WinSock2.h>
#include "vl_types.h"
#include "vl_time.hpp"
#if 1
inline vl_uint32 getTimeStampSec() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#else
inline vl_uint32 getTimeStampSec() {
  struct timeval _tv;
  gettimeofday(&_tv, NULL);
  return _tv.tv_sec;
}
#endif

inline vl_uint64 getTimeStampMS() {
  struct timeval _tv;
  gettimeofday(&_tv, NULL);
  return (unsigned long long)_tv.tv_sec * 1000 + _tv.tv_usec / 1000 ;
}

#endif
