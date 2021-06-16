#ifndef __ENDPOINT_HPP__
#define __ENDPOINT_HPP__

#include <memory>
#include <pthread.h>
#include <list>
#include "vl_types.h"
#include "vl_const.h"
#include "Socket.hpp"
#include "EptData.hpp"
#include "IOQueue.hpp"
#include "EptHeartBeat.hpp"
#include "TimerTrigger.hpp"

using namespace std;

#define ENDPOINT_DEFAULT_RECV_SIZE   (1024 * 1024)
#define ENDPOINT_DEFAULT_SEND_SIZE   (512 * 1024)

class EndPointManager;
class EndPoint;
class HBTimeoutCounter;
class HBIntervalCounter;

typedef enum {
    EEPT_NOT_READY,   // 端点正在准备阶段
    EEPT_STATE_1,     // 端点已经可以收发数据，但是不能接入管道
    EEPT_STATE_2,     // 端点可以收发数据，也可以接入管道
    EEPT_CLOSING      // 正在关闭
}EEPT_STATUS;


class HBIntervalCounter : public TickCounter {
public:
    void setOwner(shared_ptr<EndPoint> endPoint) {
        this->owner = endPoint;
    }

    HBIntervalCounter(shared_ptr<EndPoint> endPoint, const EptHeartBeat& heartbeart) :
        owner(endPoint),
        heartbeart(heartbeart),
        refCount(1) {
        struct timeval _tv;
        gettimeofday(&_tv, NULL);
        lastFireSec = _tv.tv_sec;
        lastHBRsp = _tv.tv_sec;
    }

    vl_uint32 hbInterCount;
    virtual void setMaxCount(vl_uint32 count) { this->hbInterCount = count; }
    ETIMER_TRIGGER_RET matchCond(vl_uint32 count) ;
    //  EptHeartBeat getHeartbeat() const { return heartbeart; }
    void increaseReference() { ++refCount; }
    void decreaseReference() { --refCount; }
    vl_int16 getReference() const { return refCount; }
    EptHeartBeat heartbeart;
    void fireHeartBeat();
    virtual void rtcKeepAlive();

    void updateHBRspTime()
    {
        /* 更新最后心跳时间 */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        lastHBRsp = tv.tv_sec;
        printf("last rsp time = %lu", lastHBRsp);
    }

protected:
    time_t lastHBRsp;
    time_t lastFireSec;
    shared_ptr<EndPoint> owner;
    vl_int16 refCount;
};
class HBTimeOutRetryCounter:public HBIntervalCounter
{
public:
    HBTimeOutRetryCounter(shared_ptr<EndPoint> endPoint, const EptHeartBeat& heartbeart,vl_uint16 TimeOutRetryCnt)
        :HBIntervalCounter(endPoint, heartbeart),
        hbTimeOutRetryCnt(TimeOutRetryCnt),
        curHbTimeOutRetryCnt(0),
        hbRetryTimeoutCount(0)
    {
    }

    void setMaxCount(vl_uint32 count) { HBIntervalCounter::setMaxCount(count);hbRetryTimeoutCount=count; }
    void rtcKeepAlive();
    ETIMER_TRIGGER_RET matchCond(vl_uint32 count) ;
private:
    vl_uint32 hbRetryTimeoutCount;
    vl_uint32 curHbTimeOutRetryCnt; //当前重试次数
    vl_uint32 hbTimeOutRetryCnt;     //总共重试次数
};

class HBTimeoutCounter : public TickCounter {
public:
    void setOwner(shared_ptr<EndPoint> endPoint) {
        this->owner = endPoint;
    }
    vl_uint32 hbTimeoutCount;
    void setMaxCount(vl_uint32 count) { this->hbTimeoutCount = count; }
    ETIMER_TRIGGER_RET matchCond(vl_uint32 count);
private:
    shared_ptr<EndPoint> owner;
};

/*
 * EndPoint对应一个本地端口,网络传输基类。
 * 具体实现为UdpEndPoint, TcpEndPoint
 * 在需要心跳的场景EndPoint对应一个或多个心跳，EndPoint本身有一个心跳，注册进来的IOQueue也可以有不同的心跳。
 * 现阶段场景，endpoint对应一个心跳，IOQ有独立心跳的场景暂时不实现。
 */
class EndPoint : public enable_shared_from_this<EndPoint> {
    friend class HBTimeoutCounter;
    friend class HBIntervalCounter;
    friend class EndPointManager;
public:
    /* 构造函数，可传递本地端口 */
    EndPoint();
    EndPoint(vl_uint16 basePort, ESOCK_TYPE socktype, MemoryPool* memPool ,vl_size cacheSize = 0);

    virtual ~EndPoint();

    /* 必要的初始化，暂时由子类实现，不做抽象 */
    virtual vl_status initial() = 0;
    /* 开始停止 */
    virtual vl_status stop() = 0;

    /* 对外发送数据接口 */
    virtual vl_status sendTo(vl_uint32 remoteIp, vl_uint16 remotePort, void* data, vl_size length) = 0;
    virtual vl_status sendTo(Transmittable* data) = 0;
    virtual vl_status send(void* data, vl_size length) = 0;

    /* 接收开关，开始接收后，将自动接受到的数据经过EptProtocol流入ioq */
    virtual vl_status stopRecv() = 0;
    virtual vl_status stopSend() = 0;

    vl_bool binded() {
        return sock.isNamed();
    }

    virtual int getSockFd() {
        if(sock.isOpend()) {
            return sock.getFd();
        }
        return -1;
    }

    /* 发送数据失败时，重新打开一个端口 */
    void reopen(ESOCK_TYPE socktype);

    /* 获取发送数据总量 */
    vl_size getSendedTotal() const { return sendedToc; }
    /* 获取接收数据总量 */
    vl_size getRecvedTotal() const { return recvedToc; }
    /* 获取本地端口 */
    vl_uint16 getLocalPort() const { return localPort; }
    /* 获取端点状态 */
    EEPT_STATUS getStatus() const { return status; }
    /* 心跳相关配置 */
    //void setHeartBeat(EptHeartBeat* heartBeat);

    /* 增加一个心跳 */
    void addHeartBeat(EptHeartBeat& heartBeat);
    /* 减少一个心跳 */
    void removeHeartBeat(vl_uint32 remoteIp, vl_uint16 remotePort);
    /* 防止睡眠时不发心跳，由上层一分钟定时器看护 */
    void rtcKeepAlive();
protected:
    /* ioq接入接口,不允许直接接入，需从manager中调用 */
    virtual vl_status registerSendQueue(IOQueue* ioq) = 0;
    virtual vl_status registerRecvQueue(IOQueue* ioq) = 0;
    virtual vl_status unregisterSendQueue(IOQueue* ioq) = 0;
    virtual vl_status unregisterRecvQueue(IOQueue* io) = 0;

    /* 应用层编解码器管理接口 */
    virtual vl_status addProFilter(EptProtocol* pto) = 0;
    /* 获取传输协议 */
    EptProtocol* getProtocol(EProtocol protoId) const {
        if(0 <= protoId && protoId < EPROTO_MAX) {
            return this->protocols[protoId];
        }
        else
      return NULL;
    }

    /* 没有接收队列能接收，进入缓冲 */
    vl_bool recvEncache(Transmittable* data) ;
    /* 接入队列，将延迟的数据刷入缓冲 */
    void disposeCache(IOQueue* ioq);

    inline void increaseRecvCount(vl_size count) {
        recvedToc += count;
    }
    inline void increaseSendedCount(vl_size count) {
        sendedToc += count;
    }
    /* 接收数据时使用的内存池 */
    MemoryPool* memPool;
    /* 端点状态 */
    EEPT_STATUS status;
    /* 对应一个本地端口，socket fd */
    Socket sock;
    /* 本地端口 */
    vl_uint16 localPort;
    /* 对应多条ioq, 发送队列*/
    list<IOQueue*> sendIOQs[EPROTO_MAX];
    /* 接收队列 */
    list<IOQueue*> recvIOQs[EPROTO_MAX];
    /* 应用层协议编解码 */
    EptProtocol* protocols[EPROTO_MAX];
    /* 接收数据总量 */
    vl_size recvedToc;
    /* 发送数据总量 */
    vl_size sendedToc;
    /* 缓冲大小，若缓冲大小为0，关闭缓冲功能；否则，接收到的数据如果没有ioq能及时接收，则放到缓冲队列 */
    vl_size cacheSize;
    list<Transmittable*> recvCache;
    pthread_mutex_t cacheMutex;

    /* 心跳列表执行 */
    list<HBIntervalCounter*> heartBeatExecs;
    pthread_mutex_t hbMutex;

    /* 心跳 */
    //  EptHeartBeat* pHeartBeat;
    /* 心跳间隔计数 */
    TimerTrigger* tickTrigger;
    //  HBIntervalCounter hbIntervalCounter;
    HBTimeoutCounter hbTimeoutCounter;
};

#endif










