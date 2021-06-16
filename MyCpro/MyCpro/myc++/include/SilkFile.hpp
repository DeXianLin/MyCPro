#ifndef __SILK_FILE_HPP__
#define __SILK_FILE_HPP__
#include <stdio.h>
#include "vl_types.h"
#include "AudioInfo.hpp"


/**
 * Silk 音频文件的格式化存储，解析。
 * 文件格式参考<SILK_RTP_PayloadFormat>第5节。
 * SilkStorage针对指定的文件（名），不包括文件命名策略。
 * 考虑到文件操作时会有频繁读写问题，使用std FILE.
 * 注意： 该类非线程安全类，不允许多个线程同时读写。
 */

/* 文件魔数 */
extern const char* FSILK_HEADER;

/* silk block头部 */

/*********************************************************************/
/** 注意 ： 由于其中位域跨字节，内存布局与silk文档有差别
*/

typedef enum {
  ESILK_BLOCK_8K = 0x0,
  ESILK_BLOCK_12K = 0x1,
  ESILK_BLOCK_16K = 0x2,
  ESILK_BLOCK_24K = 0x3,
  ESILK_BLOCK_MAX = 0x3,
}ESILKBLOCK;

#pragma pack(2)
class SilkBlockHeader {
#if 0
#ifdef RTP_BIG_ENDIAN
  /**
   * 000(0x0) : 8kHz
   * 001(0x1) : 12kHz
   * 010(0x2) : 16kHz
   * 011(0x3) : 24kHz
   */
  vl_uint16 mode : 3;
  /* payload字节数 */
  vl_uint16 octets : 13;
#else
  vl_uint16 octets : 13;
  vl_uint16 mode : 3;
#endif
#endif
  vl_uint16 flags;
  /* rtp时间戳 */
  vl_uint32 timestamp;
public:
  inline void setTimestamp(vl_uint32 ts) {
	this->timestamp = htonl(ts);
  }
  
  inline vl_uint32 getTimestamp() const {
	return ntohl(this->timestamp);
  }

  inline void setFlag(ESILKBLOCK mode, vl_uint16 octets) {
	this->flags = htons((mode << 13) | (octets & 0x1FFF));
  }

  inline ESILKBLOCK getMode() const {
	return (ESILKBLOCK) (ntohs(flags) >> 13);
  }

  inline vl_uint16 getOctets() const {
	return ntohs(flags) & 0x1FFF;
  }
};
#pragma pack()
  
/**
 * block 对应rtp payload中的一个包。
 * 使用场景中，payload由调用者申请。
 */
typedef struct SilkBlock {
  ESILKBLOCK spr;
  vl_uint32 length;
  vl_uint32 timestamp;
  vl_int8* payload;
}SilkBlock;

class SilkStorage {
public:
  SilkStorage(const char* path, vl_bool isRecord);
  virtual ~SilkStorage();
  /* 读取下一个silk block */
  vl_status readBlock(SilkBlock& block);
  /* 写入下一个silk block */
  vl_status writeBlock(const SilkBlock& block);

  /* 判断是否可读写 */
  vl_bool readable() const;
  vl_bool writtable() const;

  /* 
  * Set format for record. for reading ,you can not change this value.
  */
  vl_bool setFormat(EAUDIO_FORMAT fmt);
  EAUDIO_FORMAT getFormat() const { return this->format; };

  /**
   * Get magic for specific audio format
   */
  const char* getMagicStr();

  /* 重置到第一个block指针 */
  vl_bool resetOffset();
private:
  FILE* file;
  EAUDIO_FORMAT format;
  vl_bool isRecord;
};

#endif










