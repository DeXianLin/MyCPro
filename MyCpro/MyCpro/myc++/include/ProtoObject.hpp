#ifndef __PROTO_OBJECT_HPP__
#define __PROTO_OBJECT_HPP__

#include <memory.h>

#include "vl_types.h"
#include "MemoryPool.hpp"

#define __MALLOC malloc
#define __FREE free

/**
 * 网络传输应用层协议
 */
typedef enum {
  EPROTO_UNKNOW = 0,  // 未知协议类型
  EPROTO_RTP,         // rtp协议类型
  EPROTO_MAX          // 协议总数
} EProtocol;

/**
 * 协议对象抽象基类。
 * 所有的协议对象，最终均通过编码二进制传输。
 *
 * 基类主要抽象了编码缓冲区操作。
 * 一般来说，协议对象包含的内容，只是pakcet数据的指针。
 * 
 * 注意：为统一内存管理流程，packet内存统一由协议对象负责释放。
 */
class ProtoObject {
public:
  ProtoObject(vl_uint8* pktbuf, vl_size size, MemoryPool* pool = NULL) : 
	memPool(pool),
	packet(pktbuf),
	capacity(size),
	encoded(VL_FALSE),
	decoded(VL_FALSE),
	packetLen(0) {}

  /**
   * 包数据释放
   */
  virtual ~ProtoObject() {
	if(NULL != this->packet) {
	  if(NULL != memPool) {
		memPool->release(this->packet);
	  } else {
		__FREE(this->packet);
	  }
	  this->packet = NULL;
	}
	capacity = 0;
  }

  virtual void onSent() = 0;

  /**
   * 协议对象编解码状态
   */
  void setEncoded(vl_bool encoded, vl_size pktlen)  {
	this->encoded = encoded;
	if(VL_TRUE == encoded && pktlen > 0) {
	  this->packetLen = pktlen;
	}
  }
  void setDecoded(vl_bool decoded) {
	this->decoded = decoded;
  }

  /* 判断是否已经编解码 */
  vl_bool isEncoded() const { return encoded; }
  vl_bool isDecoded() const { return decoded; }

  /* Getter for packet */
  vl_uint8* getPacket() const { return this->packet; }
  vl_size getPacketCapacity() const { return this->capacity; }
  vl_size getPacketLength() const { return this->packetLen; }
  

protected:
  MemoryPool* memPool;
  /* 编码包缓冲 */
  vl_uint8* packet;
  /* 包缓冲大小 */
  vl_size capacity;
  /* 包实长 */
  vl_size packetLen;
  /* 是否已经编码，经过编码后，packet数据有效 */
  vl_bool encoded;
  /* 是否已经解码，经过解码后，数据结构化完成 */
  vl_bool decoded;
};

#endif
