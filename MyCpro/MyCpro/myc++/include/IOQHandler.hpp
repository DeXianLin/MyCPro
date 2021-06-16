#ifndef __IOQ_HANDLER_HPP__
#define __IOQ_HANDLER_HPP__

#include "vl_const.h"
#include "vl_types.h"

class IOQHandler {
public:
  IOQHandler(){}
  virtual ~IOQHandler() {};
  virtual void onIOQEvent(vl_int32 eventCode, void * eventData) = 0;
};

#endif
