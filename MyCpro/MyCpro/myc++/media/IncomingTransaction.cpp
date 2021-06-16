#include "IncomingTransaction.hpp"
#include "EndPointManager.hpp"
#include "StreamPlayScheduler.hpp"

void IncomingTransaction::initial(SockAddr& remote, shared_ptr<EndPoint> pEndPoint, const char* saveDir, TransactionInputParam& setting)
{
    reset();

    /* 创建io队列，并接入远程端点 */
    this->refEndPoint = pEndPoint;
    this->ioq = new RTPRecvIOQueue(EPROTO_RTP, remote);

    this->recordFilter = NULL;
    this->savePath = NULL;

    if(NULL != saveDir) {
        int pathLen = strlen(saveDir);

        if(pathLen > 0 && pathLen < 200) {
            pathLen += 40;
            this->savePath = (char*) malloc(pathLen);
            memset(this->savePath, 0, pathLen);
            sprintf(this->savePath, "%s/%d.sil", saveDir, setting.ssrc);

            printf("incoming transaction with record file %s", savePath);

            this->recordFilter = new RecordFilter(this->savePath);
            this->ioq->addFilter(this->recordFilter);
        }
    }

    this->circleBuffer = new UnitCircleBuffer();

    /* 上下文 */
    this->context.autoRelease = VL_FALSE;
    this->context.ssrc = setting.ssrc;

    //  AudioDecoderParam param;
    //  param.plc = setting.plc;
    //  param.bandType = setting.bandtype;
    /* 编解码 */
    this->decoder = NULL; //CodecManager::newAudioDecoder(setting.format, param);

    /* 播放线程 */
    this->pcmFeeder = new TransactionPCMFeeder(this->decoder,
                                               this->ioq,
                                               pktMemPool,
                                               &(this->context),
                                               this);

    printf("IncomingTransaction add observer with ioqueue");
    ioq->setObserver(this);
}


IncomingTransaction::IncomingTransaction(SockAddr& remote,
                                         shared_ptr<EndPoint> pEndPoint,
                                         MemoryPool* pktMemPool,
                                         const char* savePath,
                                         vl_uint32 ssrc, vl_uint32 engrossId)

    : StreamPlaySource(engrossId), pktMemPool(pktMemPool), ssrcMatcher(ssrc)

{
    TransactionInputParam param; //结构体
    param.bandtype = UNKNOWN_BAND;//TRANSACTION_DEFAULT_BANDTYPE -1
    param.format = AUDIO_FORMAT_UNKNOWN;//TRANSACTION_DEFAULT_FORMAT -1
    param.plc = VL_FALSE; //0
    param.ssrc = ssrc;

    initial(remote, pEndPoint, savePath, param);
}

IncomingTransaction::IncomingTransaction(SockAddr& remote,
                                         shared_ptr<EndPoint> pEndPoint,
                                         MemoryPool* pktMemPool,
                                         const char* savePath,
                                         TransactionInputParam& param,
                                         vl_uint32 engrossId)
    : StreamPlaySource(engrossId), pktMemPool(pktMemPool), ssrcMatcher(param.ssrc) {
    initial(remote, pEndPoint, savePath, param);
}

IncomingTransaction::~IncomingTransaction() {
    if(NULL != this->decoder) {
        delete this->decoder;
        this->decoder = NULL;
    }

    if(NULL != this->pcmFeeder) {
        delete this->pcmFeeder;
        this->pcmFeeder = NULL;
    }

    if(NULL != this->ioq) {
        if(refEndPoint != NULL) {
            //	  refEndPoint->unregisterRecvQueue(ioq);
            EndPointManager::getInstance()->unregisterRecvQueue(refEndPoint, ioq);
        }
        delete this->ioq;
        this->ioq = NULL;
    }

    if(NULL != this->recordFilter) {
        delete recordFilter;
        recordFilter = NULL;
    }

    if(NULL != savePath) {
        free(savePath);
        savePath = NULL;
    }

    if(NULL != circleBuffer) {
        delete circleBuffer;
        circleBuffer = NULL;
    }
}

void IncomingTransaction::onIOQueueUpdated() {
    //printf("IncomingTransaction onIOQueueUpdated .....................");
    if(VL_FALSE == ioq->canRead()) {

    }
    StreamPlayScheduler::getInstance()->wakeup();
}

vl_status IncomingTransaction::spsStandby() {
    return this->startTrans();
}

vl_status IncomingTransaction::spsDispose() {
    return this->stopTrans(0);
}


vl_status IncomingTransaction::spsStartTrack() {
    vl_status result = this->startTrack();
    if(result == VL_SUCCESS) {
        info_play_ts = getTimeStampMS();
    }
    return result;
}

vl_status IncomingTransaction::spsPauseTrack() {
    return this->stopTrack(VL_TRUE);
}

vl_status IncomingTransaction::spsResumeTrack() {
    return this->startTrack();
}

vl_status IncomingTransaction::spsStopTrack() {
    return this->stopTrack();
}

vl_uint32 IncomingTransaction::spsGetIdentify() const {
    return this->getSSRC();
}

SPS_STATE IncomingTransaction::spsGetPlayState() const {
    return this->getPlayState();
}

void IncomingTransaction::spsSetPlayState(SPS_STATE newState) {
    this->setPlayState(newState);
}

void IncomingTransaction::spsGetResultInfo(struct SpsResultInfo& info) const {
    struct RecvTransInfo& recvInfo = info.streamInfoDetail.recfInfo;

    info.extType = SPS_TYPE_UDP;
    this->ioq->updateAndGetSummary(recvInfo);

    /* 调用播放时间，暂时由transaction记录 */
    recvInfo.play_ts = info_play_ts;


    printf("recv summary : flag=%d, ssrc=%d, recv_id=%d, send_id=%d, group_id=%d max_seq=%d, dur_per_pack=%d",
           recvInfo.flag, recvInfo.ssrc, recvInfo.recv_id, recvInfo.send_id, recvInfo.group_id,recvInfo.max_seq, recvInfo.dur_per_pack);
    // printf("recv summary : normalEnd=%d, maxJitter=%d, totalJitter=%d, blockTimes=%d, lostPersent=%d",
    //    recvInfo.normalEnd, recvInfo.max_jitter, recvInfo.total_jitter, recvInfo.block_times, recvInfo.lost_persent);
    //printf("recv summary : frecv_ms=%llu, play_ms=%llu", recvInfo.first_recv_ts, recvInfo.play_ts);
    //printf("recv summary : send_os=%d send_net=%d", recvInfo.send_os, recvInfo.send_net_type);
}

vl_bool IncomingTransaction::spsReadyForPlay() const {
    return (SPS_WAITING == this->getPlayState()) && (this->getPredictContinuousMS() > INCOMING_ELECT_PLAY);
}

vl_bool IncomingTransaction::spsReadyForDestroy() {
    if(RTP_PT_UNKNOWN == ioq->getPayloadType()) {
        if(VL_TRUE == this->isReceiveEnd() || VL_TRUE == this->isReceiveTimeout()) {
            printf("IncomingTransaction may be too short and receive none, droped !!!!");
            spsMarkPlayed();
        }
    }
    return (SPS_PLAYED == playState) && ((VL_TRUE == this->isReceiveEnd()) || (VL_TRUE == this->isReceiveTimeout()));
}

void IncomingTransaction::spsMarkPlayed() {
    this->ioq->closeRead();
    spsSetPlayState(SPS_PLAYED);
}

vl_status IncomingTransaction::startTrans() {
    if(NULL != ioq && NULL != refEndPoint) {
        /* 为ioq添加附加匹配 */
        ioq->setPenddingMatcher(&this->ssrcMatcher);
        /* 为ioq添加结束事件 */
        ioq->setWriteEOFJudger(&this->lastPacketJudger);
        ioq->openRead();
        /* 接入端点 */
        //	refEndPoint->registerRecvQueue(ioq);
        EndPointManager::getInstance()->registerRecvQueue(refEndPoint, ioq);
        setPlayState(SPS_WAITING);
        return VL_SUCCESS;
    } else {
        return VL_ERR_TRANS_INIT;
    }
}

vl_status IncomingTransaction::stopTrans(vl_uint32 msec) {
    if(NULL != this->ioq) {
        vl_status status = ioq->asyncClose(msec);
        if(VL_SUCCESS == status) {
            return VL_SUCCESS;
        } else {
            return VL_ERR_IOQ_CLOSE_WAITING;
        }
    } else {
        printf("session release play failed, transaction not exist");
        return VL_ERR_TRANS_PREPARE;
    }
}


vl_status IncomingTransaction::startTrack() {
    if(SPS_WAITING != getPlayState() && SPS_PAUSED != getPlayState()) {
        printf("IncomingTransaction can't state track, internal state=%d", getPlayState());
        return VL_ERR_TRANS_INIT;
    }

    if(NULL == decoder && VL_TRUE == checkPayloadType(ioq->getPayloadType())) {
        printf("IncomingTrasaction get rtp type from ioq : %d", ioq->getPayloadType());
        decoder = CodecManager::createAudioDecoder(ioq->getPayloadType());
    }

    if(NULL != decoder) {
        AudioDevMgr* pAuddev = AudioDevMgr::getInstance();
        vl_status ret;

        do {
            PlayParameter plyParam;
            decoder->getEncFramePCMInfo(&plyParam.pcmInfo);
            plyParam.pcmInfo.sample_cnt;
            plyParam.streamType = -1;
            plyParam.auto_release = context.autoRelease;
            ret = pAuddev->aquirePlayer(plyParam, pcmFeeder, &audioHandler, circleBuffer);
            if(VL_SUCCESS != ret) {
                printf("Transaction aquire player failed, ret=%d", ret);
                break;
            }
            ret = pAuddev->startPlay(audioHandler);
            if(VL_SUCCESS != ret) {
                printf("transaction start failed, ret=%d", ret);
                break;
            }

            setPlayState(SPS_PLAYING);

            this->ioq->setWriteTimeOut(WRITE_TIMEOUT_FIRST_STAGE, WRITE_TIMEOUT_SECOND_STAGE);

            return VL_SUCCESS;
        } while (0);

        if(AUDDEV_INVALID_ID != audioHandler)
        {
            pAuddev->releasePlayer(audioHandler);
            audioHandler = AUDDEV_INVALID_ID;
        }
        return ret;
    } else {
        printf("transaction start play decoder is null");
        return VL_ERR_TRANS_INIT;
    }
}

vl_status IncomingTransaction::stopTrack(vl_bool isPause)
{
    //printf() << "stopTrack=================";
    if(AUDDEV_INVALID_ID != audioHandler)
    {
        AudioDevMgr* pAuddev = AudioDevMgr::getInstance();
        vl_status ret;

        ret = pAuddev->stopPlay(audioHandler, VL_FALSE);
        if(VL_SUCCESS != ret)
        {
            printf("transaction stop play failed : %d", ret);
        }
        ret = pAuddev->releasePlayer(audioHandler);
        if(VL_SUCCESS != ret)
        {
            printf("transaction release player failed : %d", ret);
        }
        audioHandler = AUDDEV_INVALID_ID;

        if(VL_TRUE == isPause)
        {
            setPlayState(SPS_PAUSED);
        } else {
            setPlayState(SPS_PLAYED);
        }
    }
    return 0;
}

vl_bool IncomingTransaction::isReceiveEnd() const {
    return ioq->getWriteStatus() == EIOQ_WRITE_END;
}

vl_bool IncomingTransaction::isReceiveTimeout() const {
    return ioq->getWriteStatus() == EIOQ_WRITE_TIMEOUT;
}

SPS_STATE IncomingTransaction::getPlayState() const {
    return playState;
}

void IncomingTransaction::setPlayState(SPS_STATE newState) {
    printf("IncomingTransaction : updatePlayState ssrc=%d, audiohandler=%d newstate=%d", this->getSSRC(), this->audioHandler, newState);
    this->playState = newState;
}

vl_uint32 IncomingTransaction::getPredictContinuousMS() const {
    if(NULL == ioq) {
        return 0;
    }
    return ioq->getPredictContinuousMS();
}

void IncomingTransaction::reset() {
    this->pcmFeeder = NULL;
    this->decoder = NULL;
    this->ioq = NULL;
    this->audioHandler = AUDDEV_INVALID_ID;
    this->circleBuffer = NULL;
}

vl_status TransactionPCMFeeder::feedPcm(PCMInfo* pcmInfo, UnitCircleBuffer* circleBuffer)
{
    Transmittable* pTrans = NULL;
    vl_bool validPcm = VL_FALSE;
    RTPObject* rtpObj = NULL;
    /**
   * 尽量一次性队列中的数据读取完
   */
    while(1)
    {
        /*
     * 若circlebuffer空间不足，直接不再取出解码，预留1s的pcm
     * 按照16kbps计算，
     */
        if((circleBuffer->getCapacity() - circleBuffer->getLength()) < 16000)
        {
            printf("IncomingTransaction circle buffer not enough space, total=%d, len=%d", circleBuffer->getCapacity(), circleBuffer->getLength());
            break;
        }

        if(NULL == (pTrans = ioq->read()))
        {
            /* 告知circlebuffer不会有新数据输入 */
            if(VL_FALSE == ioq->canRead())
            {
                if(VL_TRUE == circleBuffer->hasMoreData())
                {
                    printf("IncomingTransaction ioq read face is closed, set circleBuffer no more data !!!");
                    circleBuffer->closeWrite();
                }
            }

            /* ioq暂时无数据可读 */
            if(circleBuffer->isEmpty())
            {
                /* 已无有效数据可播放 */
                if(VL_FALSE == ioq->canRead())
                {
                    owner->setPlayState(SPS_NO_MORE);
                    printf("IncomingTransaction ioq can't read and circle buffer is empty !!!");
                    StreamPlayScheduler::getInstance()->wakeup();

                } else {
                    ioq->onReadEmpty();
                }
            }
            break;
        }

        do {
            rtpObj = dynamic_cast<RTPObject*>(pTrans->getObject());
            if(NULL == rtpObj) {
                printf("IncomingTransaction feeder rtpObj is null");
                break;
            }

            if(VL_TRUE != rtpObj->isDecoded()) {
                printf("IncomingTransaction feeder rtpObj is not decoded");
                break;
            }

            vl_uint8* payload = rtpObj->getPayload();
            vl_size payloadlen = rtpObj->getPayloadLength();

            if(NULL == payload || payloadlen <= 0)
            {
                printf("IncomingTransaction feeder, payload addr=%p, payload len=%d", payload, rtpObj->getPayloadLength());
                break;
            }

            EAUDIO_FORMAT format = PayloadTypeMapper::getCodecByPT((ERTP_PT)rtpObj->getPayloadType());
            ECODEC_BAND_TYPE bandType = PayloadTypeMapper::getBandTypeByPT((ERTP_PT)rtpObj->getPayloadType());
            //CodecManager::getCodecByPT((ERTP_PT)rtpObj->getPayloadType(), &format, &bandType);

            AudPacket * encPkt = new AudPacket((vl_int8*)payload, payloadlen, 0, this->memPool);
            encPkt->setFormat(format);
            encPkt->setTimestamp(rtpObj->getTimestamp());

            //printf("evrc payload len = %d ", payloadlen);
            audioDecoder = owner->prepareDecoder((ERTP_PT)rtpObj->getPayloadType());

            vl_status ret = VL_FAILED;

            if(audioDecoder != NULL)
            {
                ret = audioDecoder->decode(encPkt, circleBuffer);
            }

            if(VL_SUCCESS != ret)
            {
                printf("transaction feeder, decode ret=%d, sid=%d, silk size=%d",ret, rtpObj->getSequenceNumber(), encPkt->getCapacity());
                break;
            } else{

                //printf("transaction decode success, sid=%d silk size=%d", rtpObj->getSequenceNumber(), encPkt->length);
            }
            //printf("decode rtp packet success %d", rtpObj->getSequenceNumber());
            validPcm = VL_TRUE;
        } while(0);

        if(NULL != rtpObj) {
            delete rtpObj;
        }
        if(NULL != pTrans) {
            delete pTrans;
        }
    }

    //printf("feepcm======================");
    return VL_SUCCESS;
}


AudioDecoder* IncomingTransaction::prepareDecoder(ERTP_PT payloadType) {
    if(NULL != decoder) {
        /* map payload type to codec */
        EAUDIO_FORMAT newFormat = PayloadTypeMapper::getCodecByPT(payloadType);

        /* if current rpt use new encoder, drop the old decoder */
        if(newFormat != decoder->getFormatId()) {
            delete decoder;
            decoder = NULL;
        } else {
            /* if the decoder matched, update the decoder's sample rate */
            int newSR = PayloadTypeMapper::getSampleRateByPT(payloadType);

            if(newSR != decoder->getSampleRate()) {
                decoder->setSampleRate(newSR);
            }
        }
    }

    /* create new decoder if needed */
    if(NULL == decoder) {
        decoder = CodecManager::createAudioDecoder(payloadType);
    }

    return decoder;
}
