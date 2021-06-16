#ifndef __OUTGOING_TRANSACTION_HPP__
#define __OUTGOING_TRANSACTION_HPP__

#include <memory>

#include "vl_types.h"
#include "vl_const.h"

#include "IOQHandler.hpp"
#include "IOQueue.hpp"
#include "AudioDevMgr.hpp"
#include "AudioCodec.hpp"
#include "MemoryPool.hpp"
#include "RTPProtocol.hpp"
#include "EndPoint.hpp"
#include "RecordFilter.hpp"
#include "StreamPlaySource.hpp"

/**
 * Transaction剥离，将发送语音和接收语音分开，此处为发送语音类。
 * OutgoingTransaction 主要协调编码器与录音设备配置同步，支持录音过程中动态调整声音配置，如采样率，编码等级。
 */

/**
 * 发送语音配置信息，由外部模块传入或默认值。
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
  /* 一个rtp包承载的帧数量，网络情况差时，移动网络可能基于时间片策略分配发送。
   * 因此，在网络情况差时，应减少包发送频率，也就是要增大一个包包含的内容。
   * 以20毫秒为一帧计算。
  */
  vl_uint16 packetsPerRTP;
}TransactionOutputContext;


typedef enum {
  EV_OTRAN_WRTO,
  EV_OTRAN_CLTO,
  EV_OTRAN_DONE
} EV_OUTGOING_TRANS;

/**
 * 录音数据消费者
 */
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

class OutgoingTransaction : public StreamRecordSource, public IOQObserver {
public:
  /* 媒体相关配置默认 */
  OutgoingTransaction(SockAddr& remote, 
					  shared_ptr<EndPoint> pEndPoint,
					  MemoryPool* pktMemPool,
					  const char* savePath, 
					  vl_uint32 ssrc,
					  void* extension, 
					  vl_size extLen,
					  vl_uint16 extId);
  /* 指定媒体配置 */
  OutgoingTransaction(SockAddr& remote, shared_ptr<EndPoint> pEndPoint, MemoryPool* pktMemPool, const char* savePath, TransactionOutputParam& param);
  ~OutgoingTransaction();

  /* 准备开始发送语音，初始化管道 */
  vl_status startTrans();
  /* 结束语音发送，异步操作，需要等待ioq队列中的数据发送完毕或超时  */
  vl_status stopTrans(vl_uint32 msec);
  /* 开始录音，起始帧rtp序列号为0 */
  vl_status startRecord();
  /* 停止录音，最后一帧数据，设置rtp mark标志 */
  vl_status stopRecrod();
  /* 释放录音设备 */
  vl_status releaseRecord();
  
  /* 发送语音调节负载 */
  //  vl_bool resetOutgoingBandType(ECODEC_BAND_TYPE bandType);
  //  vl_bool resetOutgoingQulity(vl_uint16 qulity);

  vl_uint32 getSSRC() const { return context.ssrc; }

  vl_bool sendEmptyRTP(vl_bool hasMark);

  vl_bool readyForRelease() const { 
	if(NULL != ioq) {
	  return (ioq->getStatus() == EIOQ_CLOSED || ioq->getStatus() == EIOQ_CLOSE_TO) ? VL_TRUE : VL_FALSE;
	}
	return VL_FALSE;
  }

  /*  */
  vl_status srsStandby();
  bool srsReadyForRecord();
  vl_status srsStartRecord();
  vl_status srsStopRecord(bool async);
  bool srsReadyForDispose();
  bool srsReadyForDestroy();
  void srsDispose();
  SRS_STATE srsGetRecrodState();
  void srsSetRecordState(SRS_STATE newState);

  /* trigger when ioq update, this will help to release outgoing transaction */
  void onIOQueueUpdated();
  

  /* ioq事件监听 */
  //  void onIOQEvent(vl_int32 eventCode, void * eventData);
private:
  void reset();
  void initialOutgoing(SockAddr& remote, shared_ptr<EndPoint> pEndPoint, const char* saveDir, TransactionOutputParam& setting);
  void notifyObserver(EV_OUTGOING_TRANS ev);
  /* 通过该ioq输出语音 */
  IOQueue *ioq;
  /* 产生包数据和销毁包数据所用的内存管理 */
  MemoryPool* pktMemPool;
  /* transaction上下文信息 */
  TransactionOutputContext context;
  /* 预置编码器 */
  AudioEncoder* encoder;
  /* 启动transaction时，申请录音 */
  int audioHandler;
  /* 录音pcm数据消费者 */
  TransactionPCMComsumer* pcmConsummer;
  /* 持有远程端点引用 */
  shared_ptr<EndPoint> refEndPoint;
  /* 一个rtp包包含的语音帧数 */
  //  vl_uint32 framesPerRTP;

  /* 用于发送前置包线程 */
  pthread_t prevSendThread;

  /* 录音 */
  char* savePath;
  RecordFilter* recordFilter;

  SRS_STATE srsState;
};

#endif












