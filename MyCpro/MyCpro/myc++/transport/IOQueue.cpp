#include <stdlib.h>
#include <list>
#include <queue>
#include <time.h>
#include "vl_types.h"
#include "vl_const.h"
#include "EptProtocol.hpp"
#include "IOQueue.hpp"
#include "RTPProtocol.hpp"

using namespace std;


vl_status IOQueue::openRead() {
    readable = VL_TRUE;
    return VL_SUCCESS;
}

vl_status IOQueue::closeRead() {
    readable = VL_FALSE;
    return VL_SUCCESS;
}

vl_bool IOQueue::canRead() const {
    return readable;
}

vl_status IOQueue::closeWrite() {
    writtable = VL_FALSE;
    return VL_SUCCESS;
}

vl_status IOQueue::openWrite() {
    writtable = VL_TRUE;
    return VL_SUCCESS;
}

vl_bool IOQueue::canWrite() const {
    return writtable;
}

vl_status IOQueue::addFilter(IOFilter *filter) {
    filters.push_back(filter);
    return VL_SUCCESS;
}

vl_bool IOQueue::canAccept(const Transmittable * data) const
{
    if(VL_TRUE != writtable)
    {
        printf()<<"writtable not true===";
        return VL_FALSE;
    }

    if (NULL == data) {
        printf()<<"data is null===";
        return VL_FALSE;
    }

    /* 白名单匹配 */
    if(whiteMatcher != NULL && VL_TRUE == whiteMatcher->match(data))
    {
        printf("=====baimingdan return true");
        return VL_TRUE;
    }

    vl_bool matched = VL_FALSE;
    /* 判断协议类型，远程ip，远程端口 */
    if(dtype == data->getProtocol() && this->remoteAddr == data->getAddress())
    {
        if(NULL == penddingMatcher)
        {
            matched = VL_TRUE;
        } else if(VL_TRUE == penddingMatcher->match(data)) {
            matched = VL_TRUE;
        }else{
            printf("matcher is false====");
        }
    }
    return matched;
}

Transmittable* IOQueue::read()
{
    Transmittable* data = NULL;
    if(VL_TRUE != readable) {
        return NULL;
    }

    /* 若缓冲队列有数据，取出数据 */
    if(buffer.size() > 0)
    {
        pthread_mutex_lock(&bufLock);
        data = buffer.front();
        buffer.pop();
        pthread_mutex_unlock(&bufLock);
    }

    /* ioq已触发异步关闭 */
    //  if(EIOQ_CLOSING == status) {
    /* 写结束并且缓冲区为空 */
    if(0 == buffer.size() && VL_FALSE == writtable) {
        setStatus(EIOQ_CLOSED);//结束
    }
    //  }
    return data;
}

vl_status IOQueue::write(Transmittable * data)
{
    if (VL_TRUE == canAccept(data))
    {
        /* 遍历处理器处理数据 */
        std::list<IOFilter*>::const_iterator it;
        RTPObject* rtpObj = dynamic_cast<RTPObject*>(data->getObject());

        for(it = filters.begin(); it != filters.end(); it++) {
            (*it)->process(data);
        }

        vl_bool endedMask = VL_FALSE;

        /* 判断输入数据结束 */
        if(NULL != eofJudger && VL_TRUE == eofJudger->isWriteEOF(data)) {
            endedMask = VL_TRUE;
        }

        /* 若队列允许读出，放入缓冲区 */
        if (readable) {
            pthread_mutex_lock(&bufLock);
            buffer.push(data);
            pthread_mutex_unlock(&bufLock);

            if(rtpObj == NULL || (rtpObj->getPayloadType() != RTP_VIDEO_PAYTYPE && rtpObj->getPayloadType() != RTP_MEDIACODEC_PAYTYPE) )
                notify();
            else if(rtpObj->hasMarker() == 0)
                notify();
        }

        dirty = VL_TRUE;

        if(VL_TRUE == endedMask) {
            /* 关闭读 */
            setWriteStatus(EIOQ_WRITE_END);
            closeWrite();
        }

        return VL_SUCCESS;
    }
    return VL_ERR_IOQ_CANT_ACCEPT;
}

vl_status IOQueue::asyncClose(vl_int32 msec) { 
  TimerTrigger* globalTrigger = TimerTrigger::getInstanceSec();
    globalTrigger->unregisterWaiter(&this->closeTickCounter);


    if(EIOQ_CLOSED == this->status) {
        printf("ioq async close is already closed");
        return VL_SUCCESS;
    }

    /* 立刻关闭，关闭读写，晴空缓存 */
    if(0 == msec) {
        setStatus(EIOQ_CLOSED);
        printf("ioq force close!!!");
        return VL_SUCCESS;
    }

    /* 写结束并且缓冲为0 */
    if(VL_FALSE == writtable && buffer.size() == 0) {
        setStatus(EIOQ_CLOSED);
        printf("ioq already closed");
        return VL_SUCCESS;
    }

    /* 异步关闭 */
    status = EIOQ_CLOSING;

    /* ioq 可能仍然有数据需要传输，启动定时计数器 */
    printf("ioq async close, start tick counter to wait end");
    this->closeMaxTickCount = globalTrigger->calculate(msec);
    globalTrigger->registerWaiter(&this->closeTickCounter);

    return VL_ERR_IOQ_CLOSE_WAITING;
}

void IOQueue::setWriteTimeOut(vl_uint32 msec, vl_uint32 msecNext) {
    TimerTrigger* globalTrigger = TimerTrigger::getInstanceSec();
    globalTrigger->unregisterWaiter(&this->writeTimeoutTimer);

    if(msec > 0) {
        this->writeTimeoutMaxCount = globalTrigger->calculate(msec);
        this->writeTimeoutMaxCountNext = globalTrigger->calculate(msecNext);
        globalTrigger->registerWaiter(&this->writeTimeoutTimer);
    }
}

void IOQueue::setStatus(EIOQ_STATE status) {
    //printf("IOQueue set state from %d to %d", this->status, status);
    this->status = status;

    switch (this->status) {
    case EIOQ_READY:
        break;
    case EIOQ_CLOSING:
        break;
    case EIOQ_CLOSED:
    case EIOQ_CLOSE_TO:
        printf("IOQueue close read and wirte !!!");
        closeRead();
        closeWrite();
        break;
    }
    notify();
}

void IOQueue::notify()
{
    auto it = observers.begin();
    while(it != observers.end()) {
       //printf("IOQueue notify iter=%p===========================", *it);
        (*it)->onIOQueueUpdated();
        it++;
    }
}

ETIMER_TRIGGER_RET WriteTimerCB::matchCond(vl_uint32 count) {
    if(VL_FALSE == owner->writtable) {
        /* 数据写入完毕，注销定时刷新计数 */
        return TIMER_TRIGGER_RELASE;
    } else if(VL_TRUE == owner->dirty) {
        /* 已有数据更新，重置计数器 */
        owner->dirty = VL_FALSE;
        return TIMER_TRIGGER_RESET;
    } else {
        /* 超过最大计数，触发超时事件 */
        if(count >= owner->writeTimeoutMaxCount) {
            printf("IOQueue write timeout , count = %d", owner->writeTimeoutMaxCount);
            owner->closeRead();
            /* 第1阶段超时等待 */
            if(EIOQ_WRITING == owner->getWriteStatus()) {
                owner->setWriteStatus(EIOQ_WRITE_TIMEOUT_WAITING);
                owner->writeTimeoutMaxCount = owner->writeTimeoutMaxCountNext;
                owner->notify();
                printf("IOQueue timeout first stage !!!");
                return TIMER_TRIGGER_RESET;
            } else { /* 第2阶段超时等待 */
                owner->setWriteStatus(EIOQ_WRITE_TIMEOUT);
                owner->notify();
                printf("IOQueue timeout second stage !!!");
                return TIMER_TRIGGER_RELASE;
            }
        }
        /* 无新数据写入，增加计数 */
        return TIMER_TRIGGER_INC;
    }
}

ETIMER_TRIGGER_RET CloseTimer::matchCond(vl_uint32 count) {
    /*
  if(EIOQ_CLOSING != owner->getStatus()) {
	return TIMER_TRIGGER_RESET;
  }
  */
    printf("IOQueue close timer trigger");


    if(VL_FALSE == owner->writtable && owner->buffer.size() == 0) {
        /* IOQ已经关闭 */
        owner->setStatus(EIOQ_CLOSED);
        return TIMER_TRIGGER_RELASE;
    } else if(count >= owner->closeMaxTickCount) {
        /* IOQ关闭超时 */
        owner->setStatus(EIOQ_CLOSE_TO);
        return TIMER_TRIGGER_RELASE;
    } else {
        /* 增加计数 */
        return TIMER_TRIGGER_INC;
    }
}

