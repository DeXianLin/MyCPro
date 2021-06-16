#include "StreamPlayScheduler.hpp"

StreamPlayScheduler* StreamPlayScheduler::instance = NULL;

StreamPlayScheduler::StreamPlayScheduler() :
    wakeCond(PTHREAD_COND_INITIALIZER),
    evMutex(PTHREAD_MUTEX_INITIALIZER),
    workStateMutex(PTHREAD_MUTEX_INITIALIZER),
    working(VL_FALSE), activeSouce(NULL),
    observer(NULL), workThread(),
    preemptionSource(NULL),
    pauseState(EP_PLAY),
    waitPause(false),
    engrossId(SPS_NORMAL_PERMIT),
    recSource(NULL){

}

StreamPlayScheduler::~StreamPlayScheduler() {
    stopWork();
    pthread_cond_destroy(&wakeCond);
    pthread_mutex_destroy(&evMutex);
    pthread_mutex_destroy(&workStateMutex);
}

StreamPlayScheduler* StreamPlayScheduler::getInstance() {
    if (instance == NULL) {
        instance = new StreamPlayScheduler();
    }
    return instance;
}

void StreamPlayScheduler::terminatePlay() {
    if (NULL != activeSouce) {
        activeSouce->spsSetPlayState(SPS_NO_MORE);
        wakeup();
    }
}

void StreamPlayScheduler::terminatePlay(vl_uint32 ssrc) {
    if (NULL != activeSouce && activeSouce->spsGetIdentify() == ssrc) {
        activeSouce->spsSetPlayState(SPS_NO_MORE);
        wakeup();
    }
}

void StreamPlayScheduler::enableEngross(vl_uint32 engrossId) {
    //printf("[MUTEX] LOCK %s %p",__FUNCTION__,&evMutex);
    pthread_mutex_lock(&evMutex);
    this->engrossId = engrossId;
    pthread_mutex_unlock(&evMutex);
    //printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&evMutex);
    wakeup();
}

void StreamPlayScheduler::clearEngross() {
    printf("[MUTEX] LOCK %s %p",__FUNCTION__,&evMutex);
    pthread_mutex_lock(&evMutex);
    this->engrossId = SPS_NORMAL_PERMIT;
    pthread_mutex_unlock(&evMutex);
    printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&evMutex);
    wakeup();
}

vl_bool StreamPlayScheduler::startWork() {
    //printf("[MUTEX] LOCK %s %p",__FUNCTION__,&workStateMutex);
    pthread_mutex_lock(&workStateMutex);
    if (VL_TRUE == working) {
        return VL_TRUE;
    }

    working = VL_TRUE;
    if (0 != pthread_create(&workThread, NULL, streamplay_monitor_looper, this)) {
        working = VL_FALSE;
    }
    pthread_mutex_unlock(&workStateMutex);
    //printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&workStateMutex);
    return working;
}

void StreamPlayScheduler::stopWork() {
    printf("[MUTEX] LOCK %s %p",__FUNCTION__,&evMutex);
    pthread_mutex_lock(&evMutex);
    working = VL_FALSE;
    wakeup();
    pthread_mutex_unlock(&evMutex);
    printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&evMutex);
}

void StreamPlayScheduler::wakeup() {
    pthread_cond_signal(&wakeCond);
}


vl_bool StreamPlayScheduler::delegate(StreamRecordSource* source) {
    vl_bool result = VL_FALSE;
    printf("[MUTEX] LOCK %s %p",__FUNCTION__,&evMutex);
    pthread_mutex_lock(&evMutex);
    if(NULL != recSource) {
        if(recSource->srsReadyForDestroy()) {
            printf("StreamPlayScheduler release PREVIOUS record source");
            delete recSource;
            recSource = NULL;
        }
    }

    if(NULL == recSource) {
        if(VL_SUCCESS == source->srsStandby()) {
            recSource = source;
            result = VL_TRUE;
            wakeup();
        }
    }
    pthread_mutex_unlock(&evMutex);
    printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&evMutex);
    return result;
}

vl_bool StreamPlayScheduler::delegate(StreamPlaySource *source, vl_bool preemption) {
    if (NULL == source)
    {
        printf("StreamPlayScheduler try to delegate incoming transaction that is null !!!!!!");
        return VL_FALSE;
    }
    if (VL_TRUE != working)
    {
        printf("StreamPlayScheduler delegate when looper is not working !!!!!!!!!!");
        return VL_FALSE;
    }

    printf("StreamPlayScheduler delegate incomging ssrc=%d", source->spsGetIdentify());

    vl_bool exist = VL_FALSE;

    printf("[MUTEX] LOCK %s %p",__FUNCTION__,&evMutex);
    pthread_mutex_lock(&evMutex);
    printf("StreamPlayScheduler , lock mutex success. ");

    /* 抢占方式 */
    if (VL_TRUE == preemption) {
        preemptionSource = source;
    }


    /* 若没有找到，直接增加 */
    if (VL_FALSE == exist) {
        sourceList.push_back(source);
        source->spsStandby();
    }

    /* 唤醒调度器 */
    wakeup();
    pthread_mutex_unlock(&evMutex);
    printf("StreamPlayScheduler , unlock mutex success. ");

    if (exist == VL_TRUE) {
        return VL_FALSE;
    } else {
        return VL_TRUE;
    }
}

void* streamplay_monitor_looper(void* param) {
    if (NULL == param) {
        printf("StreamPlayScheduler looper loop failed, param = null !!!!");
        return NULL;
    }

    StreamPlayScheduler* monitor = (StreamPlayScheduler*) param;

    while (VL_TRUE == monitor->working)
    {

        struct timeval tv;
        struct timespec out;

        gettimeofday(&tv, NULL);
        out.tv_sec = tv.tv_sec + 2;
        out.tv_nsec = tv.tv_usec * 1000;
        //printf("[MUTEX] LOCK %s %p",__FUNCTION__,&monitor->evMutex);
        pthread_mutex_lock(&monitor->evMutex);
        pthread_cond_timedwait(&monitor->wakeCond, &monitor->evMutex, &out);

        if (VL_TRUE == monitor->working) {
            // 优先处理录音相关事件
            monitor->schedualRecord();
            monitor->schedualPlay();
        }

        pthread_mutex_unlock(&monitor->evMutex);
        //printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&monitor->evMutex);
    }

    return NULL;
}

void StreamPlayScheduler::schedualRecord()
{
    if(NULL != recSource) {
        SRS_STATE state = recSource->srsGetRecrodState();
        switch(state) {
        case SRS_WAITING:
            if(recSource->srsReadyForRecord()) {
                recSource->srsStartRecord();
            }
            break;
        case SRS_INITIALED:
        case SRS_INVALID:
        case SRS_RECORING:
        case SRS_STOPING:
            if(recSource->srsReadyForDispose()) {
                recSource->srsDispose();
            }
            break;
        case SRS_STOPED:
            if(recSource->srsReadyForDestroy()) {
                delete recSource;
                recSource = NULL;
            }
            break;
        }
    }
}


void StreamPlayScheduler::schedualPlay()
{

    if (EP_PAUSING == pauseState) {
        printf("schedualplay schedualPlay pausing .....");
        if (!waitPause) { // 立刻暂停
            pausePlayInternal();
        }
        pauseState = EP_PAUSED;
    } else if (EP_RESUMING == pauseState) {
        printf("schedualplay schedualPlay resuming .....");
        resumePlayInternal();
        pauseState = EP_PLAY;
    }

    /* 处理上一个播放 */
    processSourceEnd();
    //printf("schedualplay schedualPlay processSourceEnd .....");
    /* 选举下一个播放流程，有抢占的优先抢占，无抢占情况下，需要在EP_PLAY状态才选举 */
    if (NULL != preemptionSource) { // 抢占比暂停优先
        /* 处理抢占 */
        processPreemption();
    }

    if (EP_PLAY == pauseState) {
        if (SPS_NORMAL_PERMIT != engrossId) {
            /* 处理独占 */
            processEngross();
        } else {
            /* 处理普通优先级 */
            processNormal();
        }
    } else if(EP_PAUSED == pauseState) {
        /* 不缓冲接收流，将语音流标记为已播放 */
        if(VL_TRUE == dropWhenPause) {
            for(auto iter = sourceList.begin(); iter != sourceList.end(); iter ++) {
                StreamPlaySource* source = (*iter);
                if(SPS_WAITING == source->spsGetPlayState()) {
                    source->spsMarkPlayed();
                }
            }
        }
    } else {
        printf("StreamPlayScheduler schedualPlay pausing or resuming .....");
    }

    /* 回收资源 */
    if (sourceList.size() > 0) {
        list<StreamPlaySource*>::iterator iter = sourceList.begin();

        for (; iter != sourceList.end();) {
            vl_bool erased = VL_FALSE;
            StreamPlaySource* source = (*iter);

            //            if (SPS_PLAYED == source->spsGetPlayState()) {
            if (VL_TRUE == source->spsReadyForDestroy()) {
                /* 释放资源 */
                source->spsDispose();
                /* 从队列移除 */
                sourceList.erase(iter++);
                erased = VL_TRUE;

                printf("StreamPlayScheduler schedual list size = %u ", sourceList.size());

                /* 通知观察者 */
                if (NULL != observer) {
                    struct SpsResultInfo info;
                    source->spsGetResultInfo(info);
                    observer->onRelease(source->spsGetIdentify(), info);
                    printf("StreamPlayScheduler onRelease .....");
                }
                delete source;
            }
            //            }

            if (VL_FALSE == erased) {
                iter++;
            }
        }
    }
}

void StreamPlayScheduler::pausePlayInternal()
{
    printf("StreamPlayScheduler pause play");
    if (NULL != activeSouce) {
        activeSouce->spsPauseTrack();
    }
}

void StreamPlayScheduler::resumePlayInternal()
{
    printf("StreamPlayScheduler resume play");
    if (NULL != activeSouce) {
        activeSouce->spsResumeTrack();
    }
}

void StreamPlayScheduler::processSourceEnd() {
    /* 处理已播放结束的源 */
    if (NULL != activeSouce) {
        vl_bool isStopped = (SPS_NO_MORE == activeSouce->spsGetPlayState());
        vl_bool isEngross = false;
        vl_bool isPreemption = (NULL != preemptionSource);

        if (SPS_NORMAL_PERMIT != engrossId) {
            /* 处理独占 */
            if (engrossId != activeSouce->spsGetPermitId()) {
                // 存在问题：如果通话前群聊有人在说话，后面会被掐断???
                isEngross = true;
            }
        }

        if (isStopped || isPreemption || isEngross)
        {
            vl_uint32 id = activeSouce->spsGetIdentify();

            /* 释放语音设备 */
            activeSouce->spsStopTrack();
            activeSouce = NULL;

            if (NULL != observer)
            {
                observer->onPlayStoped(id);
            }
            printf("StreamPlayScheduler active source stop track !!! stop:%s, preemption:%s, engross:%s",
                   isStopped ? "true" : "false", isPreemption ? "true" : "false", isEngross ? "true" : "false");
        }
    }
}

void StreamPlayScheduler::processPreemption() {
    /* 处理抢占类型 */
    if (NULL != preemptionSource) {
        processSourceEnd(); // 掐断上一个播放

        if (VL_SUCCESS == preemptionSource->spsStartTrack()) {
            activeSouce = preemptionSource;

            if (NULL != observer) {
                observer->onPlayStarted(preemptionSource->spsGetIdentify());
            }
            printf("StreamPlayScheduler raise a Preemption source !!!");
        } else {
            printf("StreamPlayScheduler schedual to play ssrc %d failed !!!", preemptionSource->spsGetIdentify());
        }

        preemptionSource = NULL;
    }
}

void StreamPlayScheduler::processNormal() {
    /* 选举下一个可获取播放设备的会话 */
    if (NULL == activeSouce) {
        list<StreamPlaySource*>::iterator iter = sourceList.begin();

        /* 遍历托管中的会话，选出一个连续播放时长达标的会话播放 */
        while (iter != sourceList.end()) {
            if (SPS_WAITING == (*iter)->spsGetPlayState() && (*iter)->spsReadyForPlay()) {
                /* 已有一个会话达标 */
                StreamPlaySource* source = (*iter);
                if (VL_SUCCESS == source->spsStartTrack()) {
                    activeSouce = source;

                    if (NULL != observer) {
                        observer->onPlayStarted(source->spsGetIdentify());
                    }
                    printf("StreamPlayScheduler raise a listen transaction !!!");
                    break;
                } else {
                    printf("StreamPlayScheduler schedual to play ssrc %d failed !!!", source->spsGetIdentify());
                }
            }
            iter++;
        }
    }
}


void StreamPlayScheduler::processEngross() {
    /* 处理独占类型 */
    if (NULL == activeSouce) {
        list<StreamPlaySource*>::iterator iter = sourceList.begin();

        /* 遍历托管中的会话，选出一个连续播放时长达标的会话播放 */
        while (iter != sourceList.end()) {
            if (SPS_WAITING == (*iter)->spsGetPlayState() && (*iter)->spsReadyForPlay()
                && engrossId == (*iter)->spsGetPermitId()) {
                /* 已有一个会话达标 */
                StreamPlaySource* source = (*iter);
                if (VL_SUCCESS == source->spsStartTrack()) {
                    activeSouce = source;

                    if (NULL != observer) {
                        observer->onPlayStarted(source->spsGetIdentify());
                    }
                    printf("StreamPlayScheduler engross source finded and raised, eid = %d, id = %d ",
                           source->spsGetPermitId(), source->spsGetIdentify());
                    break;
                } else {
                    printf("StreamPlayScheduler engross source finded but raise failed, eid = %d, id = %d ",
                           source->spsGetPermitId(), source->spsGetIdentify());
                }
            }
            iter++;
        }
    }
}


vl_bool StreamPlayScheduler::stopRecordAsync()
{
    printf("[MUTEX] LOCK %s %p",__FUNCTION__,&evMutex);
    pthread_mutex_lock(&evMutex);
    if(NULL != recSource) {
        recSource->srsStopRecord(true);
        wakeup();
    }
    pthread_mutex_unlock(&evMutex);
    printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&evMutex);
    return true;
}
