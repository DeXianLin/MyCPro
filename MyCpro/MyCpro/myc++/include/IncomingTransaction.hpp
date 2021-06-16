#ifndef __INCOMING_TRANSACTION_HPP__
#define __INCOMING_TRANSACTION_HPP__

#include <memory>

#include "vl_types.h"
#include "vl_const.h"

#include "IOQHandler.hpp"
#include "AudioDevMgr.hpp"
#include "AudioCodec.hpp"
#include "MemoryPool.hpp"
#include "RTPProtocol.hpp"
#include "EndPoint.hpp"
#include "CodecManager.hpp"
#include "RTPJitterBuffer.hpp"
#include "RTPRecvIOQueue.hpp"
#include "RecordFilter.hpp"
#include "StreamPlaySource.hpp"

#define WRITE_TIMEOUT_FIRST_STAGE (2500)
#define WRITE_TIMEOUT_SECOND_STAGE (60 * 1000)

/**
 * 一次接收语音上下文信息，流媒体模块内部使用
 */
typedef struct {
    vl_bool autoRelease;
    vl_uint32 ssrc;
}TransactionInputContext;

typedef struct {
    vl_uint32 ssrc;
    EAUDIO_FORMAT format;
    ECODEC_BAND_TYPE bandtype;
    vl_bool plc;
}TransactionInputParam;

typedef enum {

}EV_INCOMING_TRANS;

class IncomingTransaction;
class TransactionPCMFeeder : public PCMFeeder {
public:
    TransactionPCMFeeder(AudioDecoder *audioDecoder, IOQueue *ioq,
                         MemoryPool *memPool,
                         TransactionInputContext *transParam,
                       IncomingTransaction* owner)
        : audioDecoder(audioDecoder), ioq(ioq), memPool(memPool),
        transParam(transParam), owner(owner) {
    }

    //  vl_status feedPcm(PCMInfo *pcmInfo, vl_uint8 *samples, vl_size *sample_count);
    vl_status feedPcm(PCMInfo *pcmInfo, UnitCircleBuffer* circleBuffer);
    void increaseBreackCount();
    void resetBreakCount();
private:
    AudioDecoder *audioDecoder;
    IOQueue *ioq;
    MemoryPool *memPool;
    TransactionInputContext *transParam;
    IncomingTransaction* owner;
    vl_uint32 breakCount;
};

class SSRCMatcher : public ExternalMatcher {
public:
    SSRCMatcher(vl_uint32 ssrc) : ssrc(ssrc) {}

    vl_bool match(const Transmittable * data)
    {
        const RTPObject* obj = dynamic_cast<const RTPObject*>(data->getObject());
        if(VL_TRUE == obj->isDecoded())
        {
            //printf() << "this ssrc:" << this->ssrc << "getssrc:"<<obj->getSSRC();
            if(this->ssrc == obj->getSSRC())
            {
                return VL_TRUE;
            }
        }
        printf("match obj is false");
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
                printf("IncomingTransaction last packet judger, receive last packet !!");
                return VL_TRUE;
            }
        }
        return VL_FALSE;
    }
};

class IncomingTransaction : public IOQObserver, public StreamPlaySource {
public:
    /* 群聊transaction接口，外发transaction可设置extension数据，transaction发送rtp时，会自动带到rtp附加头部中 */
    IncomingTransaction(SockAddr& remote,
					  shared_ptr<EndPoint> pEndPoint, 
					  MemoryPool* pktMemPool, 
					  const char* savePath, 
					  vl_uint32 ssrc, 
                      vl_uint32 engrossId = SPS_NORMAL_PERMIT);

    IncomingTransaction(SockAddr& remote,
					  shared_ptr<EndPoint> pEndPoint, 
					  MemoryPool* pktMemPool, 
					  const char* savePath, 
					  TransactionInputParam& param, 
                      vl_uint32 engrossId = SPS_NORMAL_PERMIT);
    ~IncomingTransaction();

    void onIOQueueUpdated();

    /* 初始化管道，接入端点接受数据 */
    vl_status startTrans();
    /* 结束接受语音，等待队列写结束或io队列超时 */
    vl_status stopTrans(vl_uint32 msec);
    /* 开始播放声音 */
    vl_status startTrack();
    /* 停止播放声音 */
    vl_status stopTrack(vl_bool isPause = VL_FALSE);

    AudioDecoder* prepareDecoder(ERTP_PT payloadType);

    vl_uint32 getSSRC() const { return context.ssrc; }
    /**
   * 获取已缓冲数据可连续播放的时间
   * 返回毫秒数
   */
    vl_uint32 getPredictContinuousMS() const;
    /**
   * 判断是否语音数据是否接收完成 / 超时
   */
    vl_bool isReceiveEnd() const;
    /**
   * 接收语音结束，判断结束原因是否因为超时。
   */
    vl_bool isReceiveTimeout() const;

    /* ========================== 用于语音调度 ========================== */
    SPS_STATE getPlayState() const;
    void setPlayState(SPS_STATE newState);
    vl_status playStandby();

    virtual vl_status spsStandby();
    virtual vl_status spsDispose();
    virtual vl_status spsStartTrack();
    virtual vl_status spsPauseTrack();
    virtual vl_status spsResumeTrack();
    virtual vl_status spsStopTrack();
    virtual vl_uint32 spsGetIdentify() const;
    virtual SPS_STATE spsGetPlayState() const;
    virtual void spsSetPlayState(SPS_STATE newState);
    virtual void spsGetResultInfo(struct SpsResultInfo& info) const;
    virtual vl_bool spsReadyForPlay() const;
    virtual vl_bool spsReadyForDestroy();
    virtual void spsMarkPlayed();
    /* ========================== 用于语音调度 ========================== */
private:
    void reset();
    void initial(SockAddr& remote, shared_ptr<EndPoint> pEndPoint, const char* saveDir, TransactionInputParam& setting);
    /* 上下文 */
    TransactionInputContext context;
    /* pcm播放线程 */
    PCMFeeder *pcmFeeder;
    /* 解码器 */
    AudioDecoder* decoder;
    /* 通过该ioq输入语音 */
    RTPRecvIOQueue *ioq;
    /* 产生包数据和销毁包数据所用的内存管理 */
    MemoryPool* pktMemPool;
    /* 持有远程端点引用 */
    shared_ptr<EndPoint> refEndPoint;
    /* 启动transaction时，申请录音 */
    int audioHandler;
    /* 用于ioq附加匹配条件 */
    SSRCMatcher ssrcMatcher;
    /* 用于判断ioq是否写入结束 */
    LastPacket lastPacketJudger;
    /* 录音功能 */
    char* savePath;
    RecordFilter* recordFilter;

    UnitCircleBuffer* circleBuffer;

    SPS_STATE playState;

    uint64_t info_play_ts;
};

#endif

















