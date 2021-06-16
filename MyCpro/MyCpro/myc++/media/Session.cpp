#include "Session.hpp"
#include "StreamPlayScheduler.hpp"

SimpleMemoryPool Session::simpleMemPool;

Session::Session(vl_uint32 rip, vl_uint16 rport, const char* root_dir, vl_uint32 engrossId, vl_uint16 lport) :
    sockAddr(rip, rport), outgoingMapLock(PTHREAD_MUTEX_INITIALIZER), incomingMapLock(PTHREAD_MUTEX_INITIALIZER), engrossId(engrossId)
{
    this->ready = defaultInitial(root_dir, lport);//初始化会话

    if(SPS_NORMAL_PERMIT != engrossId) {
        StreamPlayScheduler::getInstance()->enableEngross(this->engrossId);

    }
}

Session::Session(const char* rip, vl_uint16 rport, const char* root_dir, vl_uint32 engrossId, vl_uint16 lport) :
    sockAddr(rip, rport), outgoingMapLock(PTHREAD_MUTEX_INITIALIZER), incomingMapLock(PTHREAD_MUTEX_INITIALIZER), engrossId(engrossId) {

    this->ready = defaultInitial(root_dir, lport);

    if(SPS_NORMAL_PERMIT != engrossId) {
        StreamPlayScheduler::getInstance()->enableEngross(this->engrossId);
    }
}

Session::~Session()
{
    EndPointManager* eptMgr = EndPointManager::getInstance();
    disconnect();
    eptMgr->release(this->pEpt);

    if(engrossId != SPS_NORMAL_PERMIT)
    {
        StreamPlayScheduler::getInstance()->clearEngross();
    }
}

void Session::disconnect()
{
    printf("Session ====================== disconnect %d : %d ", sockAddr.ip(), sockAddr.port());
    EndPointManager::getInstance()->disableHeartBeat(pEpt, sockAddr.ip(), sockAddr.port());
}

vl_bool Session::defaultInitial(const char* rootDir, vl_uint16 lport) {

    /* 初始化状态 */
    recordFile = VL_TRUE;  // 是否打开本地存储，默认打开
    mute = VL_FALSE;  // 是否静音，默认关闭 否
    recording = VL_FALSE;  // 是否正在录音 否
    playing = VL_FALSE; // 是否播放状态 否

    /* 设置进入时间 */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts_in = tv.tv_sec;  // 进入该session的本地时间戳

    /* 设置群目录 */
    if(NULL != rootDir)
    {
        int len = strlen(rootDir);
        if(VL_MAX_PATH > len)
        {
            memcpy(this->root_dir, rootDir, len);
            this->root_dir[len] = 0;
        } else {
            return VL_FALSE;
        }
    }
    /* 获取本地端点，使能rtp编解码 */
    EndPointManager *eptMgr = EndPointManager::getInstance();
    pEpt = eptMgr->aquire(lport,NWK_UDP,&simpleMemPool);
    eptMgr->setupProtocol(pEpt,EPROTO_RTP);
    return VL_TRUE;

}

void Session::standby(EptHeartBeat& heartBeat)
{
    EndPointManager* eptMgr = EndPointManager::getInstance();
    eptMgr->enableHeartBeat(pEpt, heartBeat);

    printf("Session ====================== standby %d : %d ", heartBeat.getRemoteIp() ,heartBeat.getRemotePort());
}


vl_uint32 Session::createIncomingTransaction(vl_uint32 ssrc, vl_bool isLocal) //创建接收队列
{

    printf(">> createIncomingTransaction: ip:port-%s:%d", inet_ntoa(sockAddr.ip_sin()), sockAddr.port());

    IncomingTransaction* incoming = new IncomingTransaction(sockAddr, pEpt, &simpleMemPool, (const char*)this->root_dir, ssrc, engrossId);

    if(VL_TRUE == StreamPlayScheduler::getInstance()->delegate(incoming))//1
    {
        return ssrc;

    } else {
        /* 已有相同ssrc在接收队列 */
        printf("create incoming transaction failed");
        delete incoming;
        return SESSION_INVALID_HANDLER;
    }

}

SessionMsg Session::stopPlayVoice(vl_uint32 handler) {

    /*
  IncomingTransaction* incoming = findIncomingTransaction(handler);
  if(NULL == incoming) {
    LOGE("session start play failed, transaction not exist");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status ret = incoming->stopTrack();
    if(VL_SUCCESS != ret) {
        LOGE("session stop play failed, ret=%d", ret);
        return MSG_AUDDEV_ERROR;
    } else {
        return MSG_SUCCESS;
    }
    */
    return MSG_SUCCESS;
}


SessionMsg Session::releaseIncomingTransaction(vl_uint32 handler, vl_uint32 waitMsec) {
    /*
  IncomingTransaction* incoming = findIncomingTransaction(handler);
  if(NULL == incoming) {
    LOGE("session release listen failed, transaction not exist");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status status = incoming->stopTrans(waitMsec);
    if(VL_SUCCESS == status) {
        delete incoming;
        popIncomingTransaction(handler);
        return MSG_SUCCESS;
    } else if(VL_ERR_IOQ_CLOSE_WAITING == status){
        return MSG_WAIT;
    } else {
        return MSG_ERROR;
    }
    */
    return MSG_SUCCESS;
}

vl_uint32 Session::createOutgoingTransaction(TransactionOutputParam& param)
{
    //  cleanOutgoingTransaction();
    printf(">> createOutgoingTransaction: %s:%d, extensionLen=%d, extId=%d", inet_ntoa(sockAddr.ip_sin()), sockAddr.port(),
    param.extensionLen, param.extId);
    //  param.bandtype = WIDE_BAND;
    //  param.qulity = 0;
    //  param.format = AUDIO_FORMAT_UNKNOWN;
    OutgoingTransaction* outgoing = new OutgoingTransaction(sockAddr, pEpt, &simpleMemPool,(const char*)this->root_dir, param);

    if(VL_TRUE == StreamPlayScheduler::getInstance()->delegate(outgoing)) {
        return outgoing->getSSRC();
    } else {
        printf("Session delegate outgoing transaction failed");
        delete outgoing;
        return SESSION_INVALID_HANDLER;
    }
}

SessionMsg Session::startSpeak(vl_uint32 handler) {
    /*
  OutgoingTransaction* outgoing = findOutgoingTransaction(handler);
  if(NULL == outgoing) {
    LOGE("session start speak failed, transaction not exist");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status ret = outgoing->startRecord();
    if(VL_SUCCESS != ret) {
        LOGE("session start speak failed, ret=%d", ret);
        return MSG_AUDDEV_ERROR;
    } else {
        return MSG_SUCCESS;
    }
    */
        return MSG_SUCCESS;
}



// 停止录音函数由 schedualer 调用
SessionMsg Session::stopSpeak(vl_uint32 handler) {
    /*
  OutgoingTransaction* outgoing = findOutgoingTransaction(handler);
  if(NULL == outgoing) {
    LOGE("session start speak failed, transaction not exist");
    return MSG_TRANS_NOT_EXIST;
  }

    vl_status ret = outgoing->stopRecrod();
    if(VL_SUCCESS != ret) {
        LOGE("session stop speak failed, ret=%d", ret);
        return MSG_AUDDEV_ERROR;
    } else {
        return MSG_SUCCESS;
    }
    */
    StreamPlayScheduler::getInstance()->stopRecordAsync();
    return MSG_SUCCESS;
}
