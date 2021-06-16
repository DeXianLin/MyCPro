#ifndef __TRANSACTION_HPP__
#define __TRANSACTION_HPP__

#include <stdlib.h>
#include "vl_types.h"
#include "vl_const.h"
#include "IOQHandler.hpp"
#include "IOQueue.hpp"
#include "AudioDevMgr.hpp"
#include "AudioCodec.hpp"
#include "MemoryPool.hpp"
#include "RTPProtocol.hpp"

/**
 * 一个transaction只能对应一次语音，若需要双向沟通，需要两个不同的transaction
 */


/* 群聊使用的默认带宽和编码解码器，压缩等级 */
#define TRANSACTION_DEFAULT_BANDTYPE           (WIDE_BAND)
#define TRANSACTION_DEFAULT_PAYLOAD_TYPE       (RTP_PT_SILK_WB)
#define TRANSACTION_DEFAULT_QULITY             (8)

typedef enum { EMEDIA_AUDIO, EMEDIA_VIDEO } TRANS_MED_TYPE;
typedef enum { TRANSACTION_DIR_IN, TRANSACTION_DIR_OUT } TRANS_DIR;
typedef enum {
    TRANSACTION_STATE_UNINITIAL,
    TRANSACTION_STATE_ALLOCATED,
    TRANSACTION_STATE_READY,
    TRANSACTION_STATE_WORKING
} TRANS_STATE;


/**
 * 一次发送语音上下文信息，流媒体模块内部使用
 */
typedef struct {
    vl_uint32 rtpPayloadOffset;
    vl_uint32 rtpAddonSize;
    vl_uint32 maxPayloadSize;
    vl_uint32 ssrc;
    vl_uint16 seq;
    vl_uint16 rtpExtId;
    vl_size rtpExtNum;
    void* rtpExt;
}TransactionOutputContext;
/**
 * 一次接收语音上下文信息，流媒体模块内部使用
 */
typedef struct {
    vl_bool autoRelease;
    vl_uint32 ssrc;
}TransactionInputContext;

union TransactionContext {
    TransactionOutputContext output;
    TransactionInputContext input;
};

union AudioDeviceInterface {
    PCMConsumer *pcmConsumer;
    PCMFeeder *pcmFeeder;
};

union CodecInstance {
    AudioDecoder* decoder;
    AudioEncoder* encoder;
};

class TransactionPCMFeeder : public PCMFeeder {
public:
    TransactionPCMFeeder(AudioDecoder *audioDecoder, IOQueue *ioq,
                         MemoryPool *memPool,
                         TransactionInputContext *transParam)
        : audioDecoder(audioDecoder), ioq(ioq), memPool(memPool),
        transParam(transParam) {
    }

    vl_status feedPcm(PCMInfo* pcmInfo, UnitCircleBuffer* circleBuffer);

private:
    AudioDecoder *audioDecoder;
    IOQueue *ioq;
    MemoryPool *memPool;
    TransactionInputContext *transParam;
};

class TransactionPCMComsumer : public PCMConsumer {
public:
    TransactionPCMComsumer(AudioEncoder *audioEncoder, IOQueue *ioq,
                           MemoryPool *memPool,
                           TransactionOutputContext *transParam)
        : audioEncoder(audioEncoder), ioq(ioq), memPool(memPool),
        transParam(transParam) {}

    vl_status consumePcm(PCMInfo *param, const vl_uint8 *samples,
                         vl_size *sample_count);

private:
    AudioEncoder *audioEncoder;
    IOQueue *ioq;
    MemoryPool *memPool;
    TransactionOutputContext* transParam;
};

/**
 * transaction代表一次语音传输，语音传输有发送语音和接收语音两种，语音传输使用标准的rtp协议。
 * ------------------------------------------------------------------------------------------------------------
 * 发送语音时，可用参数如下：
 * ssrc : 必传
 * rtp payload type : rtp payload type 决定了使用的编解码器;
 * bandtype : 代表四种本地带宽情况，分别代表8k，12k，16k，24k采样率，同时决定了编码bitrate范围,参见ECODEC_BAND_TYPE;
 * qulity : qulity代表压缩率，默认分为10个等级，内部根据不同编解码的限制做动态调整;
 * bits per sample : 一个样本占用的比特数字，暂时默认为16bit，设置无效;
 * 
 * vl_bool dtx;            // 是否支持不连续传输
 * vl_bool cbr;            // 保持常量比特率
 * vl_bool fec;            // 帧补偿
 * ------------------------------------------------------------------------------------------------------------
 * 接收语音时，可用参数如下 :
 * ssrc ： 必传
 * bandtype
 * ------------------------------------------------------------------------------------------------------------
 * transaction 对于群聊模式，不协商流媒体参数，bandtype 采用固定值 WIDE_BAND（16k采样率）
 */

typedef struct {
    vl_uint32 ssrc;
    EAUDIO_FORMAT format;
    ECODEC_BAND_TYPE bandtype;
    vl_uint16 qulity;
    vl_bool dtx;
    vl_bool cbr;
    vl_bool fec;
    vl_uint8 bitsPerSample;
    void* extension;
    vl_size extensionLen;
    vl_uint16 extId;
}TransactionOutputParam;

typedef struct {
    vl_uint32 ssrc;
    EAUDIO_FORMAT format;
    ECODEC_BAND_TYPE bandtype;
    vl_bool plc;
}TransactionInputParam;

class SSRCMatcher : public ExternalMatcher {
public:
    SSRCMatcher(vl_uint32 ssrc) : ssrc(ssrc) {}
    vl_bool match(const Transmittable * data)
    {
        const RTPObject* obj = dynamic_cast<const RTPObject*>(data->getObject());
        if(VL_TRUE == obj->isDecoded())
        {
            printf() << "this ssrc" << this->ssrc << "getssrc" << obj->getSSRC();
            if(this->ssrc == obj->getSSRC()) {
                return VL_TRUE;
            }
        }
        return VL_FALSE;
    }
private:
    vl_uint32 ssrc;
};

class LastPacket : public ExternalJudgeEOF {
public:
    vl_bool isWriteEOF(const Transmittable* data) {
        const RTPObject* obj = dynamic_cast<const RTPObject*>(data->getObject());
        if(VL_TRUE == obj->isDecoded()) {
            /* rtp mark表示结束 */
            if(obj->hasMarker()) {
                return VL_TRUE;
            }
        }
        return VL_FALSE;
    }
};

class Transaction : public IOQHandler {
public:
    /* 群聊transaction接口，外发transaction可设置extension数据，transaction发送rtp时，会自动带到rtp附加头部中 */
    Transaction(IOQueue *ioq, MemoryPool* pktMemPool, TRANS_DIR direction, vl_uint32 ssrc, void* extension = NULL, vl_size extLen = 0, vl_uint16 extId = 0);

    /* 单聊transaction协商接口 */
    Transaction(IOQueue *ioq, MemoryPool* pktMemPool, TransactionInputParam& setting);
    Transaction(IOQueue *ioq, MemoryPool* pktMemPool, TransactionOutputParam& setting);

    void initialOutgoing(IOQueue *ioq, MemoryPool* pktMemPool, TransactionOutputParam& setting);
    void initialIncoming(IOQueue *ioq, MemoryPool* pktMemPool, TransactionInputParam& setting);

    ~Transaction();
    vl_status prepare();
    vl_status startTrans();
    vl_status stopTrans();
    vl_status release();
    vl_status addObserver();
    vl_status notify();
    vl_status isMute();

    /* 发送语音调节负载 */
    vl_status resetOutgoingBandType(ECODEC_BAND_TYPE bandType);
    vl_status resetOutgoingQulity(vl_uint16 qulity);

    void onIOQEvent(vl_int32 eventCode, void * eventData){};

    IOQueue* getIOQueue() const { return ioq; }

private:
    void reset() {
        state = TRANSACTION_STATE_UNINITIAL;
        memset((void*)&context, 0, sizeof(TransactionContext));
        memset((void*)&auddevApi, 0, sizeof(AudioDeviceInterface));
        memset((void*)&codecInstance, 0, sizeof(CodecInstance));
        ioq = NULL;
        pktMemPool = NULL;
        pAuddev = NULL;
        audioHandler = AUDDEV_INVALID_ID;
    }

    TRANS_STATE state;

    /* 代表该transaction方向 */
    TRANS_DIR direction;
    TransactionContext context;
    AudioDeviceInterface auddevApi;

    CodecInstance codecInstance;
    /* 一般为网络ioq队列 */
    IOQueue *ioq;
    /* 产生包数据和销毁包数据所用的内存管理 */
    MemoryPool* pktMemPool;

    AudioDevMgr* pAuddev;
    int audioHandler;

    /* 用于ioq附加匹配条件 */
    SSRCMatcher ssrcMatcher;

    /* 用于判断ioq是否写入结束 */
    LastPacket lastPacketJudger;
};

#endif


