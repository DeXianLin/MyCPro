#include <memory>
#include "EndPointManager.hpp"

EndPointManager* EndPointManager::instance = NULL;
/* 协议编解码器不需要重复申请释放*/
static RTPProtocol rtpProtocol;

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
shared_ptr<EndPoint> EndPointManager::aquire(vl_uint16 lport, ENWK_STREAM_TYPE nwkType, MemoryPool* memPool , vl_size cacheSize)

{
    list<EndPointRef>::iterator it = epts.begin();
    while(it != epts.end())
    {
        //zjw有相同的端点则返回该端点对象
        if((*it).endPoint->getLocalPort() == lport && (*it).nwkType == nwkType)//只支持udp
        {
            (*it).refCount ++;
            return (*it).endPoint;
        }
        it++;
    }

    /* 暂时只支持udp */
    if(NWK_UDP == nwkType)
    {
        shared_ptr<EndPoint> endPoint(new UdpEndPoint(lport, memPool, cacheSize));//创建一个对象
        endPoint->initial();//zjw端点初始化,接收和处理线程
        EndPointRef ref = {nwkType, 1, endPoint}; //结构体赋值
        pthread_mutex_lock(&eptsLock);
        epts.push_back(ref);//zjw心跳处理队列添加
        pthread_mutex_unlock(&eptsLock);
        return endPoint;
    }
}


void EndPointManager::release(shared_ptr<EndPoint> endPoint)
{
    list<EndPointRef>::iterator it = epts.begin();

    while(it != epts.end())
    {
        if((*it).endPoint == endPoint)
        {
            (*it).refCount --;
            if((*it).refCount <= 0)
            {
                endPoint->stop();
                pthread_mutex_lock(&eptsLock);
                epts.erase(it);
                pthread_mutex_unlock(&eptsLock);
            }
            break;
        }
        it++;
    }
    printf("EndPointManager::release=====");
}

void EndPointManager::disableHeartBeat(shared_ptr<EndPoint> endPoint, vl_uint32 remoteIp, vl_uint16 remotePort)
{
    endPoint->removeHeartBeat(remoteIp, remotePort);
}

void EndPointManager::setupProtocol(shared_ptr<EndPoint> endPoint, EProtocol protocolId)
{
    if(NULL != endPoint)
    {
        if(NULL == endPoint->getProtocol(protocolId))
        {
            switch(protocolId)
            {
            case EPROTO_RTP:
                endPoint->addProFilter(&rtpProtocol);
                break;
            default:
                break;
            }
        }
    }else{
        printf("=====endPoint is null====");
    }
}

void EndPointManager::enableHeartBeat(shared_ptr<EndPoint> endPoint, EptHeartBeat& heartBeat)
{
    endPoint->addHeartBeat(heartBeat);
}


void EndPointManager::rtcKeepAlive(){
    auto iter = epts.begin();

    while(iter != epts.end()) {
        (*iter).endPoint->rtcKeepAlive();
        iter ++;
    }
}

vl_status EndPointManager::registerRecvQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* ioq) {
    return applyEndpointOperation(REG_RECV, refEndPoint, ioq);
}

vl_status EndPointManager::unregisterSendQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* ioq) {
    return applyEndpointOperation(UNREG_SEND, refEndPoint, ioq);
}


vl_status EndPointManager::applyEndpointOperation(EEndPointOP op, shared_ptr<EndPoint> refEndPoint, IOQueue* ioq) {
    vl_status ret = VL_FAILED;
    if(refEndPoint == NULL || ioq == NULL) {
        return ret;
    }

    auto it = epts.begin();
    while(it != epts.end()) {
        pthread_mutex_lock(&eptsLock);
        if((*it).endPoint == refEndPoint) {
            switch(op) {
            case REG_SEND:
                refEndPoint->registerSendQueue(ioq);
                printf("EndPointManager register send queue");
                break;
            case UNREG_SEND:
                refEndPoint->unregisterSendQueue(ioq);
                printf("EndPointManager unregister send queue");
                break;
            case REG_RECV:
                refEndPoint->registerRecvQueue(ioq);
                printf("EndPointManager register recv queue");
                break;
            case UNREG_RECV:
                refEndPoint->unregisterRecvQueue(ioq);
                printf("EndPointManager unregister recv queue");
                break;
            default:
                break;
            }
            pthread_mutex_unlock(&eptsLock);
            break;
        }
        it++;
        pthread_mutex_unlock(&eptsLock);
    }
    return ret;
}

vl_status EndPointManager::unregisterRecvQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* ioq) {
    return applyEndpointOperation(UNREG_RECV, refEndPoint, ioq);
}

vl_status EndPointManager::registerSendQueue(shared_ptr<EndPoint> refEndPoint, IOQueue* ioq) {
    return applyEndpointOperation(REG_SEND, refEndPoint, ioq);
}
