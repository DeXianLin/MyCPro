#ifndef __AUDIO_INFO_HPP__
#define __AUDIO_INFO_HPP__

#include <memory.h>
#include <string.h>
#include "vl_types.h"
#include "vl_const.h"
#include "MemoryPool.hpp"

/**
 * 定义band mode，标志网络带宽情况，编解码管理器默认初始化对应的编解码实例。
 * 具体的对应关系视具体编解码器决定
 */
typedef enum ECODEC_BAND_TYPE {
    UNKNOWN_BAND = -1,  // known band type
    NARROW_BAND,        // narrowband : sample rate 8000, BR (5 - 20) kbps 低频
    MEDIUM_BAND,        // mediumband : sample rate 12000, BR (7 - 25) kbps ( will not use )中频
    WIDE_BAND,          // wideband : sample rate 16000, BR (8 - 30) kbps宽频
    SUPER_WIDE_BAND     // super-wideband : sample rate 24000, BR (20 - 40) kbps ( will not use )超宽带
}ECODEC_BAND_TYPE;


typedef enum {
    AUDIO_FORMAT_UNKNOWN = -1,
    AUDIO_FORMAT_PCM = 0,
    AUDIO_FORMAT_SILK,
    AUDIO_FORMAT_EVRC
}EAUDIO_FORMAT;

/**
 * 定义payload type，与标志rtp保持一直，silk相关pt为自定义负载类型
 */
typedef enum {
    RTP_PT_UNKNOWN = 0,
    RTP_PT_START = 96,
    RTP_PT_SILK_NB = 96,			/**< SILK narrowband/8KHz   */
    RTP_PT_SILK_MB = 97,			/**< SILK mediumband/12KHz  */
    RTP_PT_SILK_WB = 98,			/**< SILK wideband/16KHz    */
    RTP_PT_SILK_SWB = 99,			/**< SILK 24KHz		        */
    RTP_PT_EVRC_8K = 100,         /**< EVRC 8KHz		        */
    RTP_PT_EVRC_16K = 101,        /**< EVRC 16KHz		        */
    RTP_PT_END
}ERTP_PT;

inline vl_bool checkPayloadType(ERTP_PT pt) {
    if(pt >= RTP_PT_START && pt < RTP_PT_END) {
        return VL_TRUE;
    } else {
        return VL_FALSE;
    }
}

/**
 * PCM 流参数
 */
typedef struct PCMInfo {
    /* 采样率，如 8000, 16000 ... */
    vl_uint32 sample_rate;
    /* 表示语音时长 */
    vl_size  sample_cnt;
    /* 声道数，编解码暂时只支持1 */
    vl_uint32 channel_cnt;
    /* 每个sample占用比特数，编解码暂时只支持16（两个字节），后续需要更改则需要resample */
    vl_uint32 bits_per_sample;
} PCMInfo;

inline bool operator== (const PCMInfo& lhs, const PCMInfo& rhs) {
    if(lhs.sample_rate == rhs.sample_rate &&
        lhs.sample_cnt == rhs.sample_cnt &&
        lhs.channel_cnt == rhs.channel_cnt &&
        lhs.bits_per_sample == rhs.bits_per_sample)
        return true;
    return false;
}

typedef struct PCMBuffer {
    PCMInfo pcmInfo;    // 描述pcm信息
    vl_timestamp   timestamp;
    void * buf;         // pcm数据
    vl_size  capacity;  // 缓冲区大小,单位字节
} PCMBuffer;

class PayloadTypeMapper {
public:
    static ERTP_PT getPayloadType(EAUDIO_FORMAT format, ECODEC_BAND_TYPE bandType) {
        ERTP_PT pt = (ERTP_PT)-1;

        if(AUDIO_FORMAT_SILK == format) {
            switch(bandType) {
            case NARROW_BAND:
                pt = RTP_PT_SILK_NB;
                break;
            case MEDIUM_BAND:
                pt = RTP_PT_SILK_MB;
                break;
            case WIDE_BAND:
                pt = RTP_PT_SILK_WB;
                break;
            case SUPER_WIDE_BAND:
                pt = RTP_PT_SILK_SWB;
                break;
            default:
                break;
            }
        } else if(AUDIO_FORMAT_EVRC == format) {
            switch(bandType) {
            case NARROW_BAND:
                pt = RTP_PT_EVRC_8K;
                break;
            case WIDE_BAND:
                pt = RTP_PT_EVRC_16K;
                break;
            }
        }

        return pt;
    }

    static vl_bool isValidAudioFormat(EAUDIO_FORMAT format) {
        if(AUDIO_FORMAT_SILK == format || AUDIO_FORMAT_EVRC == format) {
            return VL_TRUE;
        }
        return VL_FALSE;
    }

    static vl_bool isUseBlockPacket(EAUDIO_FORMAT format) {
        vl_bool useBlock = VL_FALSE;
        switch(format) {
        case AUDIO_FORMAT_SILK:
            useBlock = VL_TRUE;
            break;
        case AUDIO_FORMAT_PCM:
        case AUDIO_FORMAT_EVRC:
            useBlock = VL_FALSE;
            break;
        }

        return useBlock;
    }

    static vl_bool isUseBlockPacket(ERTP_PT payloadType) {
        return isUseBlockPacket(getCodecByPT(payloadType));
    }

    static ERTP_PT getPayloadType(EAUDIO_FORMAT format, vl_uint32 sampleRate) {
        ERTP_PT pt = (ERTP_PT)-1;
        ECODEC_BAND_TYPE bandtype = getBandtypeBySampleRate(sampleRate);
        return getPayloadType(format, bandtype);
    }

    static EAUDIO_FORMAT getCodecByPT(ERTP_PT payloadType) {
        EAUDIO_FORMAT formatId = AUDIO_FORMAT_UNKNOWN;
        switch(payloadType) {
        case RTP_PT_SILK_NB:
        case RTP_PT_SILK_MB:
        case RTP_PT_SILK_WB:
        case RTP_PT_SILK_SWB:
            formatId = AUDIO_FORMAT_SILK;
            break;
        case RTP_PT_EVRC_8K:
        case RTP_PT_EVRC_16K:
            formatId = AUDIO_FORMAT_EVRC;
            break;
        default:
            break;
        }
        return formatId;
    }

    static ECODEC_BAND_TYPE getBandTypeByPT(ERTP_PT payloadType) {
        ECODEC_BAND_TYPE bandtype = UNKNOWN_BAND;

        switch(payloadType) {
        case RTP_PT_SILK_NB:
        case RTP_PT_EVRC_8K:
            bandtype = NARROW_BAND;
            break;
        case RTP_PT_SILK_MB:
            bandtype = MEDIUM_BAND;
            break;
        case RTP_PT_SILK_WB:
        case RTP_PT_EVRC_16K:
            bandtype = WIDE_BAND;
            break;
        case RTP_PT_SILK_SWB:
            bandtype = SUPER_WIDE_BAND;
            break;
        default:
            break;
        }
        return bandtype;
    }

    static int getSampleRateByPT(ERTP_PT payloadType) {
        return getSampleRateByBandtype(getBandTypeByPT(payloadType));
    }

    static ECODEC_BAND_TYPE getBandtypeBySampleRate(int sampleRate) {
        ECODEC_BAND_TYPE bandtype = UNKNOWN_BAND;

        switch(sampleRate) {
        case 8000:
            bandtype = NARROW_BAND;
            break;
        case 12000:
            bandtype = MEDIUM_BAND;
            break;
        case 16000:
            bandtype = WIDE_BAND;
            break;
        case 24000:
            bandtype = SUPER_WIDE_BAND;
            break;
        default:
            break;
        }
        return bandtype;
    }

    static int getSampleRateByBandtype(ECODEC_BAND_TYPE bandtype) {
        int sampleRate = 0;
        switch (bandtype) {
        case NARROW_BAND:
            sampleRate = 8000;
            break;
        case MEDIUM_BAND:
            sampleRate = 12000;
            break;
        case WIDE_BAND:
            sampleRate = 16000;
            break;
        case SUPER_WIDE_BAND:
            sampleRate = 24000;
            break;
        default:
            break;
        }
        return sampleRate;
    }
};

/**
 * 语音块打包，每个语音块中可能包含多个语音帧。
 */
class AudPacket {
public:
    /**
   * 若buffer传入null，packet将自动从memPool中申请；否则，使用传入的缓冲作为缓冲区。
   * cap: 缓冲区大小
   * headReserb : 头保留大小，改部分不做修改。
   * 注意： 传入的缓冲也必须从mempool中申请，因为对象析构，将从memPool中释放。
   * 注意： 传入的buffer徐保证4个字节对其, headerReservb也需要保证四个字节对其。
   * 不支持多线程并发操作，getLeftRange调用写如数据后，需要调用fixBlockSize修复
   */
    AudPacket();
    AudPacket(vl_int8* buffer, vl_uint32 cap, vl_uint32 headReserb, MemoryPool* memPool = NULL, vl_bool blocked = VL_TRUE)
        : buf(buffer), capacity(cap), memPool(memPool), headSize(headReserb), ready(VL_FALSE), blocked(blocked)
    {
        //printf("AudioPacket inital cap=%d, reserb=%d", cap, headReserb);
        if(cap <= headReserb) {
            return;
        }

        if(buffer == NULL) {
            if(memPool != NULL) {
                buffer = (vl_int8*)memPool->allocate(cap);
            } else {
                buffer = (vl_int8*)malloc(cap);
            }

            if(buffer == NULL) {
                return;
            }
            memset(buffer, 0, cap);
            this->buf = buffer;
        }
        curOffset = headReserb;
        ready = VL_TRUE;
    }

    /**
    * 标记特定大小的连续缓冲区将用于读或写。
    *此函数调用仅用作非块模式。
    */
    vl_int8* markRange(vl_size aquireSize) {
        if(aquireSize > getLeftSize()) {
            printf("AudPacket has not enough space to be marked !!");
            return NULL;
        }

        vl_int8* addr = this->buf + this->curOffset;
        this->curOffset += aquireSize;
        return addr;
    }

    /*
   * 获取可用的内存区域,若aquireSize为0，返回可用的所有缓冲大小。
   */
    vl_size getLeftRange(vl_int8** ptr, vl_size aquireSize = 0)
    {
        //	LOGE("audio packet left size = %d, aquire = %d", getLeftSize(), aquireSize);
        if(getLeftSize() <= 0 || aquireSize > getLeftSize()) {
            return 0;
        }

        if(getLeftSize() > aquireSize) {
            *ptr = this->buf + this->curOffset + sizeof(vl_uint16);

            if(aquireSize > 0) {
                return aquireSize;
            } else {
                return getLeftSize();
            }
        }
    }

    void fixBlockSize(vl_size blockSize) {
        if(VL_TRUE != ready) {
            printf("AudPacket fix block size when packet is not ready");
            return;
        }

        vl_uint16* blockSizePtr = (vl_uint16*)(this->buf + this->curOffset);
        *blockSizePtr = blockSize;

        /* 2字节对其 */
        curOffset += blockSize + sizeof(vl_uint16);
        if((curOffset % 2) != 0) {
            curOffset ++;
        }
    }

    vl_bool closePacket() {
        if(VL_TRUE == ready && getLeftSize() >= sizeof(vl_uint16)) {
            vl_uint16* blockSizePtr = (vl_uint16*)(this->buf + this->curOffset);
            *blockSizePtr = 0;
            this->curOffset += sizeof(vl_uint16);
            return VL_TRUE;
        } else {
            return VL_FALSE;
        }
    }

    vl_uint16 getNextBlock(vl_int8** ptr) {
        if(VL_TRUE != ready) {
            printf("AudPacket get next block when packet is not ready");
            return 0;
        }

        vl_uint16* blockSizePtr = (vl_uint16*)(this->buf + this->curOffset);
        if(*blockSizePtr == 0) {
            *ptr = NULL;
            return 0;
        } else {
            *ptr = this->buf + this->curOffset + sizeof(vl_uint16);
            this->curOffset += (sizeof(vl_uint16) + *blockSizePtr);
            if((curOffset % 2) != 0) {
                curOffset ++;
            }
            return *blockSizePtr;
        }
    }

    /**

    * 获取可用缓冲大小。
            */
        vl_uint16 getLeftSize() const {
        if(VL_TRUE == ready)
            return this->capacity - this->curOffset - sizeof(vl_uint16);
        return 0;
    }

    /**
   * 一个block的附加数据长度。
   */ 
  static vl_size getMaxBlockPaddingSize() {
        /* 用两个字节表示块长度，两个字节用于对其，理论上返回3就够了 */
        return sizeof(vl_uint16) << 1;
    }

    /**
   * 获取最大的block长度
   */
    static vl_uint16 getMaxBlockSize() {
        return (vl_uint16)(2 << 16 - 1);
    }

    vl_size getCapacity() const {
        return this->capacity;
    }

    vl_bool isReady() const {
        return this->ready;
    }

    void setTimestamp (vl_timestamp timestamp) {
        this->timestamp = timestamp;
    }
    vl_timestamp getTimestamp() const {return this->timestamp;}

    vl_size getTotalLength() const {return this->curOffset;}

    void setFormat(EAUDIO_FORMAT format) { this->format = format; }
    EAUDIO_FORMAT getFormat() const { return this->format; }

    vl_size getHeaderSize() const { return headSize; }
    vl_bool isBlocked() const { return blocked; }

    vl_int8* getBuffer() const { return buf; }

    vl_bool getContent(vl_int8** pptr, vl_size* psize) {
        *pptr = buf + headSize;
        *psize = capacity - headSize;
        return VL_TRUE;
    }

    ~AudPacket(){
        ready = VL_FALSE;
        /*
	if(NULL != this->buf) {
	  if(memPool != NULL) {
		memPool->release(this->buf);
	  } else {
		free(this->buf);
	  }
      } */
    }

public:
    /* 语音编码格式 */
    EAUDIO_FORMAT  format;
    /* 保存数据, 地址确保4字节对其 */
    vl_int8 *         buf;
    /* 最后读写指针偏移 */
    vl_size        curOffset;
    /* 缓冲区容量 */
    vl_size        capacity;
    /* 第一个sample形成的本地时间戳 */
    vl_timestamp   timestamp;
    /* 附加标志 */
    vl_uint32      bitInfo;
    /* 预留头部大小 */
    vl_uint32      headSize;
    /* 内存池 */
    MemoryPool* memPool;
    vl_bool        ready;

    /* 是否包含多个编码包 */
    vl_bool        blocked;

    //后续添加长度
    int length;
} ;

#endif




