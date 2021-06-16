#ifndef __EPT_DATA_HPP__
#define __EPT_DATA_HPP__

#include <stdlib.h>
#include <memory.h>
#include "vl_types.h"
#include "Socket.hpp"
#include "ProtoObject.hpp"

#define __MALLOC malloc
#define __FREE   free



/**
 * 可传输数据封装，包含传输内容，传输协议类型，远程端口
 */
class Transmittable {
  friend class IOQueue;
public:
  Transmittable(EProtocol protocol, SockAddr* remote, ProtoObject* obj = NULL) :
	dtype(protocol), remote(remote), obj(obj) {
  }

  EProtocol getProtocol() const {
	return dtype;
  }

  void setObject(ProtoObject * obj) {
	this->obj = obj;
  }

  ProtoObject* getObject() const { return this->obj; }

  SockAddr& getAddress() const { return *remote; }
private:
  EProtocol dtype;
  SockAddr * remote;
  ProtoObject * obj;
};

#endif










