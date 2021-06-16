#ifndef __RTP_JITTER_BUFFER_HPP__
#define __RTP_JITTER_BUFFER_HPP__

#include <queue>
#include <math.h>
#include <pthread.h>

#include "EptData.hpp"
#include "RTPProtocol.hpp"
#include "AudioInfo.hpp"
#include "Time.hpp"
#include "IOQueue.hpp"

/* 保证5个包以后才判断播放 */
#define N_MIN_DETECT_PACKET (5)
/* 保证最小缓冲400毫秒以后 */
#define N_MIN_JITTER_MSEC (400)
#define N_EXPECT_CONTINOUS (10 * 1000)
/* 接收超过该包数时，不在以最小包作为限制 */
#define N_PREDICT_DEPENDS_PACKEETS  (20)

using namespace std;

class RTPRecvIOQueue;

/**
 * 实现动态缓冲区。
 * 定义RTPObject的抖动缓冲, 抖动缓冲使用priority_queue存储，其中存放RTPObject指针。
 *
 * 累加延迟计算，设第n帧发送本地时间 Sn,接收Rn, 客户端时间差C, 延迟Dn
 * Sn + C + Dn = Rn
 * 上一个与下一个的抖动时差为
 * delta = (Rn - Sn) - (Rn-1 - Sn-1)
 * 
 * 抖动可视为加速度。
 * 在网络状况好的情况下，从一段时间看，接收速度应该与发送端速度一致，因此加速度为0。此时，
 * 抖动缓冲的数量应该比最大抖动打一些。
 * 
 * 网络拥堵的情况，接收的最终速度小于发生发送速度，加速度小于0，此时，需要计算预订一个最
 * 小顺畅播放时间，并做现行计算。
 * 
 */

/**
 * priority queue对比接口，效率考虑，该类省略了多数判断，调用者需保证对比的参数合法。
 */
class RTPTransCmp {
public:
  bool operator() (Transmittable* lhs, Transmittable* rhs){
	/*
	if(NULL == lhs || NULL == rhs) {
	  return false;
	  }
	if(EPROTO_RTP != lhs->getProtocol() || EPROTO_RTP != rhs->getProtocol()) {
	  return false;
	}
	*/
	const RTPObject* lobj = dynamic_cast<RTPObject*>(lhs->getObject());
	const RTPObject* robj = dynamic_cast<RTPObject*>(rhs->getObject());
	
	/*
	if(NULL == lobj || NULL == robj) {
	  return false;
	}
	*/

	if(lobj->getSequenceNumber() > robj->getSequenceNumber()) {
	  return true;
	}
	return false;
  }
};

class RTPJitterBuffer {
  friend class RTPRecvIOQueue;
public:
  RTPJitterBuffer(RTPRecvIOQueue* owner, vl_size dqThres = INT_MAX) : 
	owner(owner), 
	dqThreshold(dqThres), 
	jitterLock(PTHREAD_MUTEX_INITIALIZER), 
	rtpDuration(0),
	totalRTPCount(0),
	isEnded(VL_FALSE),
	totalJitter(0),
	maxJitter(0), 
	lastSendTimestamp(0),
	lastRecvTimestamp(0),
	minThreshold(N_MIN_DETECT_PACKET),
	lastSequence(0) {}
  
  ~RTPJitterBuffer() { pthread_mutex_destroy(&jitterLock); }

  /**
   * 获取一个rtp包的语音时长
   */
  vl_uint32 getRTPDurationPerPacket() const {
	return rtpDuration;
  }

  /**
   * 获取平均抖动时间
   */
  float getAverageJitter() const {
	if(totalRTPCount >= 2) {
	  return totalJitter / (float)(totalRTPCount - 1);
	} else {
	  /* 无法取得平均最差jitter */
	  return (float)INT_MAX;
	}
  }

  /**
   * 获取当前JitterBuffer缓冲的数量
   */
  vl_size getJitterBufferSize() const {
	return buffer.size();
  }

  /* 获取总jitter */
  vl_int32 getTotalJitter() const;

  /* 预测当前缓冲中能支持不断续播放的时间长度 */
  vl_int32 getPredictContinousMS() const;

  /* jitter buffer 输入 */
  void push(Transmittable* trans);
  
  /* 遇到播放断续，重新启动缓冲 */
  void rehold();

  /* 重设threshold */
  void resetThreshold(vl_size newThreshold = 0);

  vl_uint32 getLostPercent() const;
private:
  vl_int32 parseDuration(RTPObject* rtpObj);
  /**
   * 根据当前抖动情况，更新抖动罚值 
   */
  void recalculateThreshold(vl_bool force = VL_FALSE);
  /** 
   * 试图将语音推出抖动缓冲
   */
  void popupJitter();

  priority_queue<Transmittable*, vector<Transmittable*>, RTPTransCmp> buffer;
  RTPRecvIOQueue* owner;
  /* 当对头有连续dqThreshold个对象时，队头出队。 */
  vl_size dqThreshold;
  vl_size minThreshold;
  // vl_uint16 expectSeq;
  pthread_mutex_t jitterLock;
  // 是否已经收到最后一个包，不考虑乱絮
  vl_bool isEnded;

  /* 一个rtp包的流媒体时长 */
  vl_int32 rtpDuration;

  /*=======用于jitter计算 =========*/
  /* 记录最大抖动 */
  vl_int32 maxJitter;
  /* (Rn-1 - Sn-1) */
  //  vl_int32 lastDelay;
  /* 已接收RTP数量 */
  vl_int32 totalRTPCount;
  /* 总加速度 */
  vl_int32 totalJitter;
  /* 期望下一个包到达时间 */
  //  vl_uint64 expectNextTimestamp;

  vl_uint64 lastRecvTimestamp; // 接收为本地时间戳
  vl_uint32 lastSendTimestamp; // rtp 时间戳只有32bit，截断
  
  /* 最大rtp序号 */
  vl_uint16 lastSequence;
};

#endif














