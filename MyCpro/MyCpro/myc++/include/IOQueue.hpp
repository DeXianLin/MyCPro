#ifndef __IOQUEUE_HPP__
#define __IOQUEUE_HPP__

#include <stdlib.h>
#include <list>
#include <queue>
#include "vl_types.h"
#include "vl_const.h"
#include "EptData.hpp"
#include "EptProtocol.hpp"
#include "IOQHandler.hpp"
#include "IOFilter.hpp"
#include "TimerTrigger.hpp"

#define IOQ_CLOSING_WATI_TIME     (3000)

class IOQObserver {
public:
	virtual ~IOQObserver(){};
	virtual void onIOQueueUpdated() = 0;
    virtual int onData(unsigned char *src, int nsize, unsigned int &tp, int & info1, int & info2){return 0;};
	virtual void enActive(){}
	virtual void deActive(){}
};


/**
 * IOQueue异步关闭说明
 *
 * ioq关闭读，ioq将删除当前缓冲，后续数据不缓冲。
 * ioq可由设置的内部判断接口判断ioq是否写入结束，并置位writeFinish标志，也可由写入方调用setWriteFinish接口设置。
 *
 * ioq初始化时候，默认写打开，读关闭。若缓冲读出，需要在接入写端前，打开读接口。
 * 关闭写接口，代表ioq写入结束。
 *
 * 当writeFinish标志置位并且ioq为空时，可判定为ioq可关闭。
 */

/**
 * 定义ioq异步事件
 */
typedef enum {
  IOQ_EV_IN,          // IOQ 有数据数据
  IOQ_EV_IN_END,      // IOQ 收到结束数据
  IOQ_EV_OVER_BUF,    // IOQ 缓冲数量超过水位
  IOQ_EV_WRTO,        // IOQ 超过一定时间没有数据写入（在网络模块，可产生网络超时事件）
  IQQ_EV_CLTO,        // IOQ 异步关闭超时
  IOQ_EV_DONE         // IOQ 数据读写完成
}EV_IOQ;

/**
 * IOQueue用于网络数据读写缓冲，数据Filter处理。
 *
 * IOQ写入时，会在写入线程做filter处理。并将处理的结果放入指定的队列。
 * 由于处理机制问题，IOQ写入和读出的数据类型应该保持一致。
 * 因此也限制ioq中的filter也只能使用同样的数据类型
 */
typedef struct ExternData {
    vl_uint32 send_id;
    vl_uint32 recv_id;         // 群聊代表目标群id，个人代表个人uin
    vl_uint32 flag :8;         // 0 点对点， 1 群聊
    vl_uint32 net_type : 4;    // 发送端网络信息：0表示WIFI，2表示2G，3表示3G
    vl_uint32 os_type : 4;     // 发送端平台 : 0 表示android， 1 表示ios
    vl_uint32 reserve :16;
} ExternData;

typedef enum {
  EIOQ_READY,    // ioq准备完成，根据设置，可以读写
  EIOQ_CLOSING,  // ioq正在关闭中，不能继续写入
  EIOQ_CLOSED,   // 初始化未完成或ioq已经关闭
  EIOQ_CLOSE_TO  // ioq关闭超时
}EIOQ_STATE;

typedef enum {
  EIOQ_WRITING,
  EIOQ_WRITE_TIMEOUT_WAITING,
  EIOQ_WRITE_END,
  EIOQ_WRITE_TIMEOUT
} EIOQ_WRITE_STATE;

using namespace std;

/**
 * 外部匹配接口
 */
class ExternalMatcher {
public:
  virtual vl_bool match(const Transmittable*) = 0;
};

/**
 * ioq外部匹配是否写结束
 */
class ExternalJudgeEOF{
public:
  virtual vl_bool isWriteEOF(const Transmittable*) = 0;
};

class IOQueue;

/* 写超时计数 */
class WriteTimerCB : public TickCounter {
public:
  WriteTimerCB() {}
  ETIMER_TRIGGER_RET matchCond(vl_uint32 count);
  void setOwner(IOQueue* owner) { this->owner = owner; }
private:
  IOQueue* owner;
};

/* 关闭超时计数 */
class CloseTimer : public TickCounter {
public:
  ETIMER_TRIGGER_RET matchCond(vl_uint32 count);
  void setOwner(IOQueue* owner) { this->owner = owner; }
private:
  IOQueue* owner;
};

class IOQueue
{
  friend class WriteTimerCB;
  friend class CloseTimer;
public:
  /**
   * 每条ioq对应EndPoint中的一个外网ip端口，并指定一种应用层协议。
   * proto [in] : 应用层协议用于确定产生的EptData的协议类型。
   * rip [in] : 用于确认在该EndPoint上对应的外网ip
   * rport [in] : 用于确定在该EndPoint上对应的外网端口
   */
  IOQueue(EProtocol proto, vl_uint32 rip, vl_uint16 rport) : dtype(proto), remoteAddr(rip, rport) {
	defaultInitial();
  }


  IOQueue(EProtocol proto, const char * rip, vl_uint16 rport) : dtype(proto), remoteAddr(rip, rport) {
	defaultInitial();
  }

  IOQueue(EProtocol proto, SockAddr& remoteAddr) : dtype(proto), remoteAddr(remoteAddr) {
	defaultInitial();
  }

  /**
   * 解绑设置进来的Filter列表，后续使用smart ptr来管理生命周期。
   */
  virtual ~IOQueue() {
	/* 注销写超时计数 */
	TimerTrigger* globalTrigger = TimerTrigger::getInstanceSec();
	globalTrigger->unregisterWaiter(&this->writeTimeoutTimer);
	globalTrigger->unregisterWaiter(&this->closeTickCounter);
	
	this->readable = VL_FALSE;
	this->writtable = VL_FALSE;

	whiteMatcher = NULL;
	penddingMatcher = NULL; 
	eofJudger = NULL;

	/* 释放filter */
	filters.clear();

	/* 清空缓存数据 */
	while(this->buffer.size() > 0) {
	  pthread_mutex_lock(&bufLock);
	  Transmittable* trans = buffer.front();
	  buffer.pop();
	  pthread_mutex_unlock(&bufLock);
	  delete trans;
	}

	pthread_mutex_destroy(&bufLock);
  }
  
  EProtocol getProtocal() const {
	return dtype;
  }

  unsigned int getBufferdSize() const {
	return buffer.size();
  }

  /* 设置外部匹配器 */
  void setPenddingMatcher(ExternalMatcher* matcher) { this->penddingMatcher = matcher; }
  void setWhiteMatcher(ExternalMatcher* matcher) { this->whiteMatcher = matcher; }

  /* 外部判断是否已写入结束数据 */
  void setWriteEOFJudger(ExternalJudgeEOF* judger) { this->eofJudger = judger; }

  /* 设置ioq观察者 */
  void setObserver(IOQObserver* observer) {
    //printf("IOQueue notify = ==========================  push observer=%p", observer);
    observers.push_back(observer);
  }

  /**
   * 打开读端，需要做数据缓冲
   */
  virtual vl_status openRead();
  /**
   * 关闭读端，数据写入后，只经过filter处理，不做缓存
   */
  virtual vl_status closeRead();
  /**
   * 判断IOQ是否可以读
   */
  virtual vl_bool canRead() const;
  /**
   * 写关闭，某些情况下充当锁操作，写入操作返回错误
   */
  virtual vl_status closeWrite();
  virtual vl_status openWrite();
  /**
   * 判断IOQ是否可写
   */
  virtual vl_bool canWrite() const;
  /**
   * 添加数据操作的filter对象，filter有序
   */
  virtual vl_status addFilter(IOFilter* filter);
  /**
   * 判断该IOQueue是否可以接受指定数据
   */
  virtual vl_bool canAccept(const Transmittable *) const;
  /**
   * 读出数据，读取失败返回NULL
   */
  virtual Transmittable* read();
  /* 读操作无数据可读时触发 */
  virtual void onReadEmpty() {}
  /**
   * 写入数据，写入成功返回VL_SUCCESS
   */
  virtual vl_status write(Transmittable *);
  /**
   * 等待ioq写入结束，ioq关闭操作并不会立刻释放。
   * 若ioq已有写入结束标志，返回VL_SUCCESS, 此时ioq中可能还有可读数据，若直接删除ioq，缓存中的数据将丢失。
   * 否则，返回VL_ERR_IOQ_CLOSE_WAITING，如果在指定时间内ioq能顺利关闭,则回调返回VL_ERR_IOQ_CLOSED.
   * 若IOQ在指定时间内无法顺利关闭，回调返回VL_ERR_IOQ_CLOSE_TIMEOUT.
   * msec : [in] msec指定等待关闭的最大时间，单位毫秒
   */
  virtual vl_status asyncClose(vl_int32 msec);
  /**
   * IOQ事件通知IOQHandler
   */
  virtual void notify();
  /**
   * 设置写入超时时间，使用全局计数器计数。
   */
  void setWriteTimeOut(vl_uint32 msec, vl_uint32 msecNext);

  vl_uint32 getRemmteIp() {
	return remoteAddr.ip();
  }
  vl_uint16 getRemotePort() {
	return remoteAddr.port();
  }
  SockAddr& getRemoteAddr() {
	return remoteAddr;
  }

  EIOQ_STATE getStatus() const { return this->status; }

  EIOQ_WRITE_STATE getWriteStatus() const { return this->writeStatus; }
  void setWriteStatus(EIOQ_WRITE_STATE writeStatus) { this->writeStatus = writeStatus; } 

  void setDirty() { dirty = VL_TRUE; }
protected:
  /* 默认初始化 */
  void defaultInitial() {
	/* 默认写打开，读关闭 */
	writtable = VL_TRUE;
	readable = VL_FALSE;

	whiteMatcher = NULL;
	penddingMatcher = NULL; 
	eofJudger = NULL;

	dirty = VL_FALSE;
	writeTimeoutMaxCount = 0;
	writeTimeoutMaxCountNext = 0;

	/* 注意，此处在构造阶段传出this指针，be care */
	closeTickCounter.setOwner(this);
	writeTimeoutTimer.setOwner(this);
	status = EIOQ_READY;

	pthread_mutex_init(&bufLock, NULL);

    //	observer = NULL;
    observers.clear();
	writeStatus = EIOQ_WRITING;
  }

  void setStatus(EIOQ_STATE status);

  /* 目标地址 */
  SockAddr remoteAddr;

  /* 协议类型 */
  EProtocol dtype;

  /* 队列是否可读 */
  vl_bool readable;
  /* 队列是否可写 */
  vl_bool writtable;
  /* IOQ状态 */
  EIOQ_STATE status;
  /* 写队列超时全局触发 */
  WriteTimerCB writeTimeoutTimer;
  vl_bool dirty;
  vl_uint32 writeTimeoutMaxCount;
  vl_uint32 writeTimeoutMaxCountNext;
  EIOQ_WRITE_STATE writeStatus;
  /* 关闭超时全局触发 */
  CloseTimer closeTickCounter;
  vl_uint32 closeMaxTickCount;
  /* 白名单匹配 */
  ExternalMatcher* whiteMatcher;
  /* 附加匹配 */
  ExternalMatcher* penddingMatcher;
  /* 外部写结束判断 */
  ExternalJudgeEOF* eofJudger;
  /* 过滤器链表 */
  std::list<IOFilter*> filters;
  /* 缓冲队列 */
  std::queue<Transmittable*> buffer;
  /* 多线程锁 */
  pthread_mutex_t bufLock;
  /* ioq观察者 */
  list<IOQObserver*> observers;
};



#endif













