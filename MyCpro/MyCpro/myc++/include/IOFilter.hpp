#ifndef __IOFILTER_HPP__
#define __IOFILTER_HPP__

#include "EptData.hpp"

/**
 * 数据过滤器接口,负责处理数据。数据处理的输入和输出应该为同种类型。
 */
class IOFilter {
public:
  IOFilter() {
  }
  virtual ~IOFilter() {
  }
  virtual void process(Transmittable * data) = 0;
};

#endif
