#ifndef __MEMORY_POOL_HPP__
#define __MEMORY_POOL_HPP__

#include <stdlib.h>
#include "vl_types.h"


class MemoryPool {
public:
  virtual ~MemoryPool() {}
  virtual void* allocate(vl_size size) = 0;
  virtual void release(void* ptr) = 0;
};

class SimpleMemoryPool : public MemoryPool {
public:
  void* allocate(vl_size size) {
	return malloc((size_t) size);
  }
  void release(void* ptr) {
	free(ptr);
  }
};

#endif












