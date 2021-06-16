#include "EndPoint.hpp"
#include "EptHeartBeat.hpp"
#include "IOQHandler.hpp"
#include "IOQueue.hpp"

extern "C" {
//void notifyMediaHeartbeatReceive(int i);
void onMediaHeartbeatFailed();
}


EndPoint::EndPoint(vl_uint16 basePort, ESOCK_TYPE socktype, MemoryPool* memPool ,vl_size cacheSize) :
    recvedToc(0), sendedToc(0), memPool(memPool), cacheSize(cacheSize), cacheMutex(PTHREAD_MUTEX_INITIALIZER),
    hbMutex(PTHREAD_MUTEX_INITIALIZER),
    localPort(basePort),sock(Socket(socktype)), status(EEPT_NOT_READY), tickTrigger(NULL) {

    printf("kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
    printf()<<EPROTO_MAX;
    for(int i = 0; i < EPROTO_MAX; i ++) {
        protocols[i] = NULL;
    }

    if(VL_TRUE == sock.isOpend()) {
        sock.setRecvBufferSize(ENDPOINT_DEFAULT_RECV_SIZE);
        sock.setSendBufferSize(ENDPOINT_DEFAULT_SEND_SIZE);

        if(0 != basePort) {
            if(VL_SUCCESS != sock.bind(new SockAddr(basePort))) {
                printf("EndPoint bind failed");
            } else {
                status = EEPT_STATE_1;
            }
        }
    }
}

EndPoint::~EndPoint() {


    printf("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm");

    if(NULL != this->tickTrigger) {
        tickTrigger->unregisterWaiter(&hbTimeoutCounter);
        //	tickTrigger->unregisterWaiter(&hbIntervalCounter);
    }
    pthread_mutex_lock(&hbMutex);
    list<HBIntervalCounter*>::iterator iter = heartBeatExecs.begin();

    while(iter != heartBeatExecs.end())
    {
        TimerTrigger* globalTrigger = TimerTrigger::getInstanceSec();
        HBIntervalCounter* hbCounter = (*iter);

        heartBeatExecs.erase(iter++);
        printf("oooooooooooooooooooooooooooooooooo");
        globalTrigger->unregisterWaiter(hbCounter);
        //delete hbCounter; //zjw add
    }
    pthread_mutex_unlock(&hbMutex);

    pthread_mutex_lock(&cacheMutex);
    list<Transmittable*>::iterator it = recvCache.begin();
    while(it != recvCache.end())
    {
        Transmittable* temp = *it;
        it = recvCache.erase(it);

        if(temp != NULL) {
            if(temp->getObject() != NULL) {
                delete temp->getObject();
            }
            delete temp;
        }
    }
    pthread_mutex_unlock(&cacheMutex);
    pthread_mutex_destroy(&cacheMutex);
    pthread_mutex_destroy(&hbMutex);
}

void EndPoint::removeHeartBeat(vl_uint32 remoteIp, vl_uint16 remotePort) {
    pthread_mutex_lock(&hbMutex);

    list<HBIntervalCounter*>::iterator iter = heartBeatExecs.begin();

    while(iter != heartBeatExecs.end())
    {
        vl_bool erased = VL_FALSE;

        printf("EndPoint remoteIp = %d , remotePort = %d cachedIp = %d cachedPort = %d ",remoteIp, remotePort,
               (*iter)->heartbeart.getRemoteIp(), (*iter)->heartbeart.getRemotePort());

        if((*iter)->heartbeart.getRemoteIp() == remoteIp && (*iter)->heartbeart.getRemotePort() == remotePort) {
            (*iter)->decreaseReference();

            if((*iter)->getReference() <= 0) {
                HBIntervalCounter* hbCounter = (*iter);
                heartBeatExecs.erase(iter++);

                TimerTrigger* globalTrigger = TimerTrigger::getInstanceSec();
                globalTrigger->unregisterWaiter(hbCounter);
                //delete hbCounter;  //zjw add
                erased = VL_TRUE;
            }
        }

        if(VL_FALSE == erased) {
            iter++;
        }
    }
    pthread_mutex_unlock(&hbMutex);
    printf("removeHeartBeat end=======");
}

void EndPoint::reopen(ESOCK_TYPE socktype) {
    return;
}

vl_bool EndPoint::recvEncache(Transmittable* data) {
    if(cacheSize > 0) {
        if(recvCache.size() < cacheSize) {
            pthread_mutex_lock(&cacheMutex);
            recvCache.push_back(data);
            pthread_mutex_unlock(&cacheMutex);
        } else {
            pthread_mutex_lock(&cacheMutex);
            recvCache.pop_front();
            recvCache.push_back(data);
            pthread_mutex_unlock(&cacheMutex);
        }
        return VL_TRUE;
    } else {
        return VL_FALSE;
    }
}

void EndPoint::disposeCache(IOQueue* ioq) {
    list<Transmittable*>::iterator it = recvCache.begin();

    while(it != recvCache.end())
    {
        if(ioq->canAccept((*it))) {
            ioq->write(*it);
            printf("ept lock 111");
            pthread_mutex_lock(&cacheMutex);
            printf("ept lock 222");
            it = recvCache.erase(it);
            printf("ept lock 333");
            pthread_mutex_unlock(&cacheMutex);
            printf("ept lock 444");
        }
        it++;
    }
}

ETIMER_TRIGGER_RET HBIntervalCounter::matchCond(vl_uint32 count) {
    if(count >= hbInterCount)
    {
        return TIMER_TRIGGER_RESET;
    } else {
        return TIMER_TRIGGER_INC;
    }
}

ETIMER_TRIGGER_RET HBTimeOutRetryCounter::matchCond(vl_uint32 count) {

    HBIntervalCounter::matchCond(count);
    if(!this->hbRetryTimeoutCount)
    {
        return TIMER_TRIGGER_RESET;
    }
    if(count >= this->hbRetryTimeoutCount) {
        if(lastHBRsp < lastFireSec && curHbTimeOutRetryCnt < hbTimeOutRetryCnt)
        {
            //指定的超时时间没有收到心跳回复则重试
            fireHeartBeat();
            curHbTimeOutRetryCnt++;
            printf("time out hb response from %d,%d,try cnt:%d, timeout:%d, firehbtime:%lu, last reponse time%lu" ,
                   heartbeart.getRemoteIp(),heartbeart.getRemotePort() , curHbTimeOutRetryCnt , this->hbRetryTimeoutCount,lastFireSec,lastHBRsp);
            this->hbRetryTimeoutCount++;
            return TIMER_TRIGGER_RESTART;
        }
        if(curHbTimeOutRetryCnt >= hbTimeOutRetryCnt)
        {
            //如果重试次数超过默认的次数则停止重试
            printf("pHbIntervalCounter time out hb response from %d,%d, has retryed %d times give up" ,
                   heartbeart.getRemoteIp(),heartbeart.getRemotePort(),curHbTimeOutRetryCnt);
            this->hbRetryTimeoutCount = 0;
            curHbTimeOutRetryCnt = 0;
            //onMediaHeartbeatFailed();
        }
        return TIMER_TRIGGER_RESET;
    } else {
        return TIMER_TRIGGER_INC;
    }
}

ETIMER_TRIGGER_RET HBTimeoutCounter::matchCond(vl_uint32 count) {
    return TIMER_TRIGGER_RELASE;}

extern void notifyMediaHeartbeatTimeout();

void HBIntervalCounter::fireHeartBeat()
{
    void* data;
    vl_size len;
    heartbeart.getHearBeat(&data, &len);
    /* 发送心跳 */
    heartbeart.markHeartBeat();
    owner->sendTo(heartbeart.getRemoteIp(), heartbeart.getRemotePort(), data, len);
    printf("fireHeartBeat !");
    /* 更新最后心跳时间 */
    struct timeval _tv;
    gettimeofday(&_tv, NULL);

    /* 若心跳发送间隔超过一分钟， 通知上层 */
    if(((_tv.tv_sec - lastFireSec) > 80) || ((_tv.tv_sec - lastHBRsp) > 80)) {
        printf("report media hb timeout,last heart beat dist=%ld, from last resp=%ld", _tv.tv_sec-lastFireSec, _tv.tv_sec-lastHBRsp);
        //notifyMediaHeartbeatTimeout();
        lastHBRsp = _tv.tv_sec;
    }

    lastFireSec = _tv.tv_sec;
}

void EndPoint::addHeartBeat(EptHeartBeat& heartBeat)
{
    pthread_mutex_lock(&hbMutex);

    /* 判断在已有的注册列表中没有相同的heartBeat */
    list<HBIntervalCounter*>::iterator iter = heartBeatExecs.begin();

    while(iter != heartBeatExecs.end())
    {
        /* 理论上只要判断 */
        if(((*iter)->heartbeart.getRemoteIp() == heartBeat.getRemoteIp() && (*iter)->heartbeart.getRemotePort() == heartBeat.getRemotePort())) {
            (*iter)->increaseReference();
            break;
        }
        iter ++;
    }

    vl_uint32 ip = heartBeat.getRemoteIp();
    printf("EndPoint addHeartBeat remote ip = %d.%d.%d.%d remote port = %d ", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
         (ip >> 8) & 0xFF,(ip) & 0xFF, heartBeat.getRemotePort());

    /* 没有相同的远程心跳 */
    if(iter == heartBeatExecs.end())
    {
        HBIntervalCounter* hbCounter = new HBTimeOutRetryCounter(shared_from_this(), heartBeat,5);//new HBIntervalCounter(shared_from_this(), heartBeat);
        TimerTrigger* globalTrigger = TimerTrigger::getInstanceSec();

        void* data;
        vl_size len;
        heartBeat.getHearBeat(&data, &len);
        /* 流媒体心跳首次发送，发两次 */
        heartBeat.markHeartBeat();
        sendTo(heartBeat.getRemoteIp(), heartBeat.getRemotePort(), data, len);
        heartBeat.markHeartBeat();
        sendTo(heartBeat.getRemoteIp(), heartBeat.getRemotePort(), data, len);

        /* 设置心跳间隔计数 */
        vl_uint32 tickCount = globalTrigger->calculate(0.5 * 1000);//globalTrigger->calculate(heartBeat.getInterval() * 1000);
        hbCounter->setMaxCount(tickCount);
        globalTrigger->registerWaiter(hbCounter);//有安卓操作忽略了

        heartBeatExecs.push_back(hbCounter);
    } else {
        printf("EndPoint add heartbeat failed, same remote ip and port heartbeat exist !!!");
    }

    pthread_mutex_unlock(&hbMutex);
}

void HBIntervalCounter::rtcKeepAlive() {
    struct timeval _tv;
    gettimeofday(&_tv, NULL);
    //  heartbeart.triggerLog();
    //  heartbeart.resetBeatLog();
    fireHeartBeat();
    /*

    if((_tv.tv_sec - lastFireSec) > 20) {
        LOGI("endpoint rtc keep alive to fire heart, last=%ld, current=%ld", lastFireSec, _tv.tv_sec);
        fireHeartBeat();
    } else {
        LOGI("endpoint rtc keep alive but not fire, last=%ld, current=%ld", lastFireSec, _tv.tv_sec);
    }
    */
}

void HBTimeOutRetryCounter::rtcKeepAlive()
{
    //上层调用触发了心跳,重新开始重试计数
    this->count = 0;
    curHbTimeOutRetryCnt = 0;
    this->hbRetryTimeoutCount = hbInterCount;
    HBIntervalCounter::rtcKeepAlive();
}


void EndPoint::rtcKeepAlive()
{
    pthread_mutex_lock(&hbMutex);
    auto iter = heartBeatExecs.begin();
    printf("Endpoint trigger rtc keep alive from alarm");
    while(iter != heartBeatExecs.end()) {
        (*iter)->rtcKeepAlive();
        iter ++;
    }
    pthread_mutex_unlock(&hbMutex);
}
