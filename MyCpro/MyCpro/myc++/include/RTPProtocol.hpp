#ifndef __RTPPACKET_HPP__
#define __RTPPACKET_HPP__

#include "vl_types.h"
#include "EptProtocol.hpp"
#include "ProtoObject.hpp"
#include "MemoryPool.hpp"
#include "Time.hpp"

#define RTP_VERSION 2
#define RTP_MAXCSRCS 15
#define RTP_MINPACKETSIZE 600
#define RTP_DEFAULTPACKETSIZE 1400
#define RTP_PROBATIONCOUNT 2
#define RTP_MAXPRIVITEMS 256
#define RTP_SENDERTIMEOUTMULTIPLIER 2
#define RTP_BYETIMEOUTMULTIPLIER 1
#define RTP_MEMBERTIMEOUTMULTIPLIER 5
#define RTP_COLLISIONTIMEOUTMULTIPLIER 10
#define RTP_NOTETTIMEOUTMULTIPLIER 25
#define RTP_DEFAULTSESSIONBANDWIDTH 10000.0

#define RTP_RTCPTYPE_SR 200
#define RTP_RTCPTYPE_RR 201
#define RTP_RTCPTYPE_SDES 202
#define RTP_RTCPTYPE_BYE 203
#define RTP_RTCPTYPE_APP 204

#define RTP_MEDIACODEC_PAYTYPE 103
#define RTP_VIDEO_PAYTYPE 102

typedef struct RTPHeader {
#ifdef RTP_BIG_ENDIAN
  vl_uint8 version : 2;
  vl_uint8 padding : 1;
  vl_uint8 extension : 1;
  vl_uint8 csrccount : 4;

  vl_uint8 marker : 1;
  vl_uint8 payloadtype : 7;
#else  // little endian
  vl_uint8 csrccount : 4;
  vl_uint8 extension : 1;
  vl_uint8 padding : 1;
  vl_uint8 version : 2;

  vl_uint8 payloadtype : 7;
  vl_uint8 marker : 1;
#endif // RTP_BIG_ENDIAN
  vl_uint16 sequencenumber;
  vl_uint32 timestamp;
  vl_uint32 ssrc;
} RTPHeader;

struct RTPExtensionHeader {
  uint16_t extid;
  uint16_t length;
};

struct RTPSourceIdentifier {
  uint32_t ssrc;
};

struct RTPEncParam {
  vl_size extnum;
  vl_size csrcnum;
};

/**
 * RTP协议解析类。
 * 编码时：
 */
class RTPProtocol : public EptProtocol {
public:
  RTPProtocol() : EptProtocol(EPROTO_RTP) {}
  /**
   * rtp编码为简单的封装编码，packet内存直接引用ProtoObject的缓冲内存
   */
  vl_status encode(ProtoObject *obj, void *packet, vl_size* pktlen);
  vl_status decode(ProtoObject *obj, void *packet, vl_size pktlen);

  ProtoObject* allocObject(void* packet, vl_size pktlen);
  vl_bool parse(void *packet, vl_size pktlen);
  void freeObject(ProtoObject*);

  static vl_size getPrefixSize(void* param) ;
  static vl_size getPostfixSize(void* param) { return 0; };
};

/**
 * rtp
 */
class RTPObject : public ProtoObject {
  friend class RTPProtocol;

public:
  RTPObject(vl_uint8 *pktbuf, vl_size size, MemoryPool* pool = NULL) : ProtoObject(pktbuf, size, pool)
  {
    hasextension = VL_FALSE;
    hasmarker = VL_FALSE;
    numcsrcs = 0;
	csrcs = NULL;
	csrcs_mem = VL_FALSE;
    payloadtype = 0;
	sequence = 0;
    timestamp = 0;
    ssrc = 0;
    payload = NULL;
    payloadlength = 0;
    extid = 0;
    extension = NULL;
	extension_mem = VL_FALSE;
    extensionlength = 0;
  }

  ~RTPObject() {
    if (NULL != this->csrcs && VL_TRUE == this->csrcs_mem) {
      __FREE(this->csrcs);
    }
    if (NULL != this->extension && VL_TRUE == this->extension_mem) {
      __FREE(this->extension);
    }
  }

  /**
   * 设置payload偏移和大小
   */
  void setPayloadByOffset(vl_size offset, vl_size size) {
    if (NULL != this->packet) {
      this->payload = this->packet + offset;
      this->payloadlength = size;
    }
  }

  void setCSRCs(vl_uint8 numcsrcs) {
	if (NULL != this->csrcs && VL_TRUE == this->csrcs_mem) {
      __FREE(this->csrcs);
    }
	this->csrcs_mem = VL_FALSE;
	if(numcsrcs > 0) {
	  this->numcsrcs = numcsrcs;
	  this->csrcs = (vl_uint32*)(getPacket() + sizeof(RTPHeader));
	} else {
	  this->numcsrcs = 0;
	  this->csrcs = NULL;
	}
  }

  /**
   * 复制csrc数据
   */
  void copyCSRCs(vl_uint8 numcsrcs, const vl_uint32 *csrcs) {
    if (NULL != this->csrcs && VL_TRUE == this->csrcs_mem) {
      __FREE(this->csrcs);
    }
	if(numcsrcs > 0) {
	  this->numcsrcs = numcsrcs;
	  this->csrcs_mem = VL_TRUE;
	  this->csrcs = (vl_uint32 *)__MALLOC(numcsrcs);
	  for (int i = 0; i < numcsrcs; i++) {
		*(this->csrcs + i) = *(csrcs + i);
	  }
	} else {
	  this->numcsrcs = 0;
	  this->csrcs_mem = VL_FALSE;
	  this->csrcs = 0;
	}
  }

  void setExtension(vl_uint16 extensionid, vl_uint16 number, const vl_uint32 offset) {
    if (NULL != this->extension && VL_TRUE == this->extension_mem) {
      __FREE(this->extension);
	  this->extension = NULL;
    }

	this->extension_mem = VL_FALSE;
	if(0 == number) {
	  this->hasextension = VL_FALSE;
	} else {
	  if(offset < sizeof(RTPHeader)) {
		this->hasextension = VL_FALSE;
        printf("set extension offset failed, offset=%d", offset);
		return;
	  } else {
		this->extid = extensionid;
		this->hasextension = VL_TRUE;
		this->extensionlength = number * sizeof(vl_uint32);
		this->extension = (vl_uint8*)this->packet + offset;
	  }
	}
  }

  /**
   * 复制附加数据
   */
  void copyExtension(vl_uint16 extensionid, vl_uint16 number,
                     const void *extension) {
    if (NULL != this->extension && VL_TRUE == this->extension_mem) {
      __FREE(this->extension);
    }
    if (NULL != extension && 0 != number) {
      this->hasextension = VL_TRUE;
      this->extid = extensionid;
	  this->extension_mem = VL_TRUE;
      this->extension = (vl_uint8*)__MALLOC(number * sizeof(vl_uint32));
      this->extensionlength = number * sizeof(vl_uint32);
      memcpy(this->extension, extension, this->extensionlength);
    } else {
      this->hasextension = VL_FALSE;
      this->extensionlength = 0;
      this->extension = NULL;
    }
  }

  /**
   * 附加数据保存在外部，调用该函数持有外部附加数据指针，调用encode前，外部指针不能释放，否则附加数据出错
   */
  void attachExternalExtension(vl_uint16 extensionId, vl_uint16 number, void* extension) {
	if (NULL != this->extension && VL_TRUE == this->extension_mem) {
	  __FREE(this->extension);
	  this->extension_mem = VL_FALSE;
	}
	
	if(number > 0 && NULL != extension ) {
	  this->hasextension = VL_TRUE;
	  this->extid = extensionId;
	  this->extension = (vl_uint8*)extension;
	  this->extensionlength = sizeof(vl_uint32) * number;
	} else {
	  this->hasextension = VL_FALSE;
	}
  }

  void onSent() {
	RTPHeader *rtphdr = (RTPHeader*) this->getPacket();
	rtphdr->timestamp = htonl(::getTimeStampSec());
  }

  void setPayloadType(vl_uint8 pt) { this->payloadtype = pt; }
  void setMark(vl_bool mark) { this->hasmarker = mark; } 
  void setSSRC(vl_uint32 ssrc) { this->ssrc = ssrc; }
  void setSequence(vl_uint16 number) { this->sequence = number; }
  void setTimestamp(vl_uint32 timestamp) { this->timestamp = timestamp; }

  vl_bool hasMarker() const { return hasmarker; }

  vl_uint8 getPayloadType() const { return payloadtype; }
  vl_size getPayloadLength() const { return payloadlength; }
  vl_uint8 *getPayload() const { return payload; }

  vl_uint32 getSSRC() const { return ssrc; }
  vl_uint16 getSequenceNumber() const {
    return (uint16_t)(sequence & 0x0000FFFF);
  }

  vl_size getCSRCCount() const { return numcsrcs; }
  vl_uint32* getCSRCArray() const {
	return csrcs;
  }
  vl_uint32 getCSRC(int num) const {
    if (num >= numcsrcs)
      return 0;

    vl_uint8 *csrcpos;
    vl_uint32 *csrcval_nbo;
    vl_uint32 csrcval_hbo;

    csrcpos = packet + sizeof(RTPHeader) + num * sizeof(vl_uint32);
    csrcval_nbo = (vl_uint32 *)csrcpos;
    csrcval_hbo = ntohl(*csrcval_nbo);
    return csrcval_hbo;
  }

  vl_uint32 getTimestamp() const { return timestamp; }

  vl_bool hasExtension() const { return hasextension; }
  vl_uint16 getExtensionID() const { return extid; }
  vl_uint8 *getExtensionData() const { return extension; }
  vl_size getExtensionLength() const { return extensionlength; }

private:
  vl_bool hasextension;
  vl_bool hasmarker;
  vl_uint8 payloadtype;
  vl_uint16 sequence;
  vl_uint32 timestamp;
  vl_uint32 ssrc;

  /* 外部数据，若实例由rtp流解析得来，该值应当与payload一致或设为null */
  vl_uint8 *payload;
  vl_size payloadlength;

  vl_uint16 extid;
  vl_uint8 *extension;
  vl_size extensionlength;
  /* 标记extension指向的内存是否为内部额外申请，设置该标志，析构时自动释放 */
  vl_bool extension_mem;

  vl_int32 numcsrcs;
  vl_uint32 *csrcs;
  /* 标记csrcs指向的内存是否内部申请 */
  vl_bool csrcs_mem;

protected:
  /**derived from ProtoObject
  vl_uint8 *packet;
  vl_size packetLen;
  vl_size capacity;
  vl_bool encoded;
  vl_bool decoded;
  */
};
#endif

