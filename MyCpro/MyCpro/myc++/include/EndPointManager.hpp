#ifndef __ENDPOINT_MANAGER_HPP__
#define __ENDPOINT_MANAGER_HPP__

#include <list>
#include <memory>

#include "EndPoint.hpp"
#include "UdpEndPoint.hpp"
#include "RTPProtocol.hpp"

typedef enum {
    NWK_TCP,
    NWK_UDP
} ENWK_STREAM_TYPE;

typedef struct {
    ENWK_STREAM_TYPE nwkType;
    int refCount;
    shared_ptr<EndPoint> endPoint;
} EndPointRef;

enum EEndPointOP {
    REG_SEND,
    UNREG_SEND,
    REG_RECV,
    UNREG_RECV
};

/**
 * 管理本地网络端点。
 * 现逻辑如下：
 * 获取一个本地网络端点，若指定的本地端口已初始化，则返该EndPoint，并增加引用计数。
 * 若该本地端口未管理，则尝试创建一个endpoint,引用计数为1，创建错误返回null；
 * 返回EndPoint时，减少引用次数，若引用计数为0，释放EndPoint.
 *
 * 注意： 考虑本地端口暂时不会使用多个，只是直接把EndPoint地址保存到list链表中。
 *        暂时只支持udp网络  
 */
class EndPointManager {
public:
    static EndPointManager* getInstance() {
        if(NULL == instance) {
            instance = new EndPointManager();
        }
        return instance;
    }

    vl_status registerSendQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* ioq);
    vl_status registerRecvQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* ioq);
    vl_status unregisterSendQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* ioq);
    vl_status unregisterRecvQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* io);

    shared_ptr<EndPoint> aquire(vl_uint16 lport, ENWK_STREAM_TYPE nwkType, MemoryPool* memPool , vl_size cacheSize = 512);
    void release(shared_ptr<EndPoint> endPoint);
    void setupProtocol(shared_ptr<EndPoint> endPoint, EProtocol protocolId);
    /* 接口待扩展 */
    void enableHeartBeat(shared_ptr<EndPoint> endPoint, EptHeartBeat& heartbeat);
    void disableHeartBeat(shared_ptr<EndPoint> endPoint, vl_uint32 remoteIp, vl_uint16 remotePort);

    void rtcKeepAlive();

    ~EndPointManager() {
        releaseAllEndPoint();
        pthread_mutex_destroy(&eptsLock);
    }
private:
    vl_status applyEndpointOperation(EEndPointOP op, shared_ptr<EndPoint> refEndPoint, IOQueue* ioq);

    EndPointManager() : eptsLock(PTHREAD_MUTEX_INITIALIZER) {}
    void releaseAllEndPoint();
    static EndPointManager* instance;
    list<EndPointRef> epts;
    pthread_mutex_t eptsLock;
};

#endif











