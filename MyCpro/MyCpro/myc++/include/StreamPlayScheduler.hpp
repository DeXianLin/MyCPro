#ifndef __TRANSACTION_MONITOR_HPP__
#define __TRANSACTION_MONITOR_HPP__

#include <pthread.h>
#include <list>
#include <memory>
#include "vl_types.h"
#include "StreamPlaySource.hpp"
#include "Session.hpp"


using namespace std;

#define INCOMING_ELECT_PLAY (20 * 1000)

extern void* streamplay_monitor_looper(void* param);

enum AudioPauseState {
  EP_PLAY,
  EP_PAUSING,
  EP_PAUSED,
  EP_RESUMING
};

/**
 * 流媒体播放观察者
 */
class StreamPlayObserver
{
public:
  //virtual void onSetup(vl_uint32 ssrc) = 0;
  /*
   * 开始接收流媒体数据事件
   */
  virtual void onReceiveStart(vl_uint32 ssrc) = 0;
  /* 
   * 结束接收流媒体数据事件
   */
  virtual void onReceiveEnd(vl_uint32 ssrc) = 0;
  /**
   * 流媒体处理完毕，并已经释放占用资源
   */
  virtual void onRelease(vl_uint32 ssrc,struct SpsResultInfo& info) = 0;
  /**
   * 开始播放流媒体事件
   */
  virtual void onPlayStarted(vl_uint32 ssrc) = 0;
  /**
   * 结束播放流媒体事件
   */
  virtual void onPlayStoped(vl_uint32 ssrc) = 0;
};


class StreamPlayScheduler
{
  friend void* streamplay_monitor_looper(void* param);
public:
  ~StreamPlayScheduler();

  /* 委托IncomingTransaction */
  vl_bool delegate(StreamPlaySource* source, vl_bool preemption = VL_FALSE);

  /* 委托StreamRecordSource,若delegate返回失败，source需自行释放 */
  vl_bool delegate(StreamRecordSource* source);

  /* 单例 */
  static StreamPlayScheduler* getInstance();
  void wakeup();
  void wakeupSeraial();

  /* 启动处理线程 */
  vl_bool startWork();
  /* 结束处理线程 */
  void stopWork();

  /* 声音播放调度 */
  void schedualPlay();

  /* 录音调度 */
  void schedualRecord();

  /* 内部暂停播放 */
  void pausePlayInternal();
  void resumePlayInternal();

  /* 终止播放一个声音源 */
  void terminatePlay();
  void terminatePlay(vl_uint32 ssrc);

  /* 独占id */
  void enableEngross(vl_uint32 engrossId);
  void clearEngross();

  /* 处理无数据源 */
  void processSourceEnd();

  /* 处理抢占 */
  void processPreemption();

  /* 处理一般数据源 */
  void processNormal();

  /* 处理独占 */
  void processEngross();

  /* 设置监听器 */
  void setObserver(StreamPlayObserver* observer) { this->observer = observer; }

  /* 暂停声音接口
	 canDelay参数代表是否等待当前播放结束；
	 drop代表暂停过程中，新语音流是否直接丢弃；
  */
  vl_bool pausePlay(vl_bool canDelay = VL_FALSE, vl_bool drop = VL_TRUE);
  vl_bool resumePlay();
  AudioPauseState getPauseState() const { return pauseState; }

  /* 异步停止录音接口 */
  vl_bool stopRecordAsync();

private:
  pthread_t workThread;
  pthread_cond_t wakeCond;
  pthread_mutex_t evMutex;
  pthread_mutex_t workStateMutex;

  /* 当前托管的录音对象 */
  StreamRecordSource* recSource; 

  /* 抽象实时语音流与文件播放 */
  list<StreamPlaySource*> sourceList;
  StreamPlaySource* activeSouce;

  /* 抢占源 */
  StreamPlaySource* preemptionSource;

  StreamPlayObserver* observer;
  vl_bool working;

  /* 暂停声音调度，接收语音不会中断 */
  AudioPauseState pauseState;
  vl_bool waitPause;
  vl_bool dropWhenPause;

  vl_uint32 engrossId;

  StreamPlayScheduler();
  static StreamPlayScheduler* instance;
};

#endif

