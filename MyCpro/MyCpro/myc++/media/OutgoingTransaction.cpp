#include <memory>

#include "Time.hpp"
#include "RTPProtocol.hpp"
#include "CodecManager.hpp"
#include "OutgoingTransaction.hpp"
#include "EndPointManager.hpp"
#include "StreamPlayScheduler.hpp"

// DELAY ： 耗时操作  编码，evrc约几十毫秒，silk约10~20毫秒
vl_status TransactionPCMComsumer::consumePcm(PCMInfo *pcmInfo,
                                             const vl_uint8 *samples,
                                             vl_size *sample_count)
{
    //printf("consumePcm=======================");
    if(NULL == audioEncoder)
    {
        printf("pcm consume failed, audioEncoder is null");
        return VL_FAILED;
    }

    //printf("DELAY DEBUG : opensl callback ms=%lld", getTimeStampMS());

    int bytesPerSample = pcmInfo->bits_per_sample >> 3;
    if(VL_FALSE == audioEncoder->enqueuePCM((const void *)samples, (*sample_count) * bytesPerSample)) {
        printf("pcm consume failed, audioEncoder enqueuePCM failed");
        return VL_FAILED;
    }

    while(VL_TRUE == audioEncoder->readyToGetPacket())
    {
        /* packet 中要包含每个语音包的包头，包头存放对应包的长度 */
        vl_uint32 pktbuflen = transParam->maxPayloadSize + transParam->rtpAddonSize;
        vl_uint8* pktbuf = (vl_uint8*)memPool->allocate(pktbuflen);
        if(NULL == pktbuf) {
            printf("pcm consume failed, alloc pkt buf failed");
            return VL_SUCCESS;
        }

        //printf("OutgoingTransaction payload offset = %d", transParam->rtpPayloadOffset);

        AudPacket* encPkt = new AudPacket((vl_int8*)pktbuf, pktbuflen, transParam->rtpPayloadOffset, NULL);
        if(NULL == encPkt) {
            printf("pcm consume alloc packet failed");
            memPool->release(pktbuf);
            return VL_SUCCESS;
        }
        if(VL_TRUE != encPkt->isReady()) {
            printf("pcm consume failed, alloc packet failed");
            memPool->release(pktbuf);
            delete encPkt;
            return VL_SUCCESS;
        }

        encPkt->setTimestamp(getTimeStampSec());

        if(VL_SUCCESS != audioEncoder->getEncodedPacket(encPkt)) {
            memPool->release(pktbuf);
            delete encPkt;
            printf("pcm consume failed, getEncodedPacket failed");
            break;
        }

        RTPObject * rtpObj = new RTPObject(pktbuf, pktbuflen, memPool);
        rtpObj->setPayloadType(PayloadTypeMapper::getPayloadType(audioEncoder->getFormatId(), audioEncoder->getSampleRate()));
        rtpObj->setSSRC(transParam->ssrc);
        rtpObj->setSequence(transParam->seq++);
        rtpObj->setTimestamp(encPkt->getTimestamp());
        rtpObj->setPayloadByOffset(transParam->rtpPayloadOffset, encPkt->getTotalLength() - encPkt->getHeaderSize());
        /* 有附加数据 */
        if(0 != transParam->rtpExtNum && NULL != transParam->rtpExt) {
            rtpObj->attachExternalExtension(transParam->rtpExtId, transParam->rtpExtNum, transParam->rtpExt);
        }

        rtpObj->setDecoded(VL_TRUE);

        Transmittable* transPtr = new Transmittable(EPROTO_RTP, &ioq->getRemoteAddr(), rtpObj);
        //	printf("DELAY DEBUG : rtp send %d, ms=%lld", rtpObj->getSequenceNumber(),getTimeStampMS());
        /* 进入ioq之后，生命周期由ioq管理 */

        if(VL_SUCCESS != ioq->write(transPtr))
        {
            printf("transaction consume pcm, write to ioq failed");
            memPool->release(pktbuf);
            delete rtpObj;
            delete transPtr;
        }

        delete encPkt;
        encPkt = NULL;
    }
    return VL_SUCCESS;
}

/**
 * 根据网络情况，确定一个网络传输包包含的语音长度，单位毫秒。
 */
vl_uint32 adjustTransPacketMs(ECODEC_BAND_TYPE bandtype, vl_uint16 qulity) {
    /* 5包 / s */
    // 临时固定发送方语音
    return 200;
}

void OutgoingTransaction::reset() {
    memset(&context, 0, sizeof(context));
    audioHandler = AUDDEV_INVALID_ID;
    encoder = NULL;
    pcmConsummer = NULL;
    ioq = NULL;
    srsState = SRS_INVALID;
}


// DELAY ： 耗时操作 打开录音文件，约十几毫秒

void OutgoingTransaction::initialOutgoing(SockAddr& remote, shared_ptr<EndPoint> pEndPoint, const char* saveDir, TransactionOutputParam& setting) {
    reset();

    setting.format = AUDIO_FORMAT_SILK;
    if(AUDIO_FORMAT_SILK == setting.format) {
        AudioEncoderParam encParam;
        encParam.sampleRate = 8000;
        encParam.packageSampleCount = 8000/5;
        encParam.dtx = VL_FALSE;
        encParam.cbr = VL_FALSE;
        encParam.fec = VL_FALSE;

        switch(setting.bandtype) {
        case WIDE_BAND:
        case SUPER_WIDE_BAND:
            encParam.bitrate = 15000;
            break;
        case NARROW_BAND:
        case MEDIUM_BAND:
        default:
            encParam.bitrate = 12000;
            break;
        }
        printf("OutgoingTransaction create: codec=silk, bitrate=%d", encParam.bitrate);
        /* 初始化编码器 */
        this->encoder = CodecManager::createAudioEncoder(setting.format, encParam);
    } else {
        // 临时固定发送方语音
        setting.bandtype = NARROW_BAND;
        setting.qulity = 0;
        printf("OutgoingTransaction create: evrc, auto select param");
        this->encoder = CodecManager::createAudioEncoder(setting.format);
    }

    // NOTE : delay here
    if(NULL == this->encoder) {
        return;
    }

    /* 创建io队列，并接入远程端点 */
    this->refEndPoint = pEndPoint;
    this->ioq = new IOQueue(EPROTO_RTP, remote);


    this->recordFilter = NULL;
    this->savePath = NULL;

    if(NULL != saveDir) {
        int pathLen = strlen(saveDir);

        if(pathLen > 0 && pathLen < 200) {
            pathLen += 40;
            this->savePath = (char*) malloc(pathLen);
            memset(this->savePath, 0, pathLen);
            sprintf(this->savePath, "%s/%d.sil", saveDir, setting.ssrc);

            printf("outgoing transaction with record file %s", savePath);

            this->recordFilter = new RecordFilter(this->savePath);
            this->ioq->addFilter(this->recordFilter);
        }
    }

    /* 初始化傻姑娘下文，若需要带上附加数据，增加payload偏移，复制附加数据到上下文 */
    int extension_number = 0;
    if(setting.extension != NULL && setting.extensionLen > 0) {
        /* 个数四个字节对其 */
        printf("OutgoingTransaction rtp extension len=%d",setting.extensionLen);
        extension_number = setting.extensionLen / sizeof(vl_uint32) + ((0 == (setting.extensionLen % sizeof(vl_uint32))) ? 0 : 1);

        this->context.rtpExtNum = extension_number;
        this->context.rtpExtId = setting.extId;

        /* 复制附加数据 */
        this->context.rtpExt = malloc(setting.extensionLen);
        memcpy(this->context.rtpExt, setting.extension , setting.extensionLen);
    }

    /* 计算一个rtp包包含多少个语音帧 */
    //  this->framesPerRTP = adjustTransPacketMs(setting.bandtype, setting.qulity) / encoder->getFrameMS();
    //  printf("Outgoing transaction adjust %d frames per rtp", this->framesPerRTP);

    /* 上下文 */
    RTPEncParam rtpEncParam;
    rtpEncParam.extnum = extension_number;
    rtpEncParam.csrcnum = 0;

    this->context.ssrc = setting.ssrc;
    this->context.seq = 0;
    this->context.rtpPayloadOffset = RTPProtocol::getPrefixSize(&rtpEncParam);
    this->context.rtpAddonSize = RTPProtocol::getPrefixSize(&rtpEncParam) + RTPProtocol::getPostfixSize(NULL);
    /* 获取一帧的最大数值*当前网络一个包的帧数 */
    this->context.maxPayloadSize = encoder->getMaxPayloadSize(); //(encoder->getMaxPayloadSize() + AudPacket::getMaxBlockPaddingSize()) * this->framesPerRTP;
    //  printf("initial outgoing transaction, frames=%d, maxPayloadSize = %d",this->framesPerRTP, this->context.maxPayloadSize);

    this->pcmConsummer = new TransactionPCMComsumer(encoder, ioq, pktMemPool, &context);

    printf("OutgoingTransaction add observer with ioqueue");
    ioq->setObserver(this);
    srsSetRecordState(SRS_INITIALED);
}

OutgoingTransaction::OutgoingTransaction(SockAddr& remote,
                                         shared_ptr<EndPoint> pEndPoint,
                                         MemoryPool* pktMemPool,
                                         const char* savePath,
                                         vl_uint32 ssrc,
                                         void* extension,
                                         vl_size extLen,
                                         vl_uint16 extId)
    : pktMemPool(pktMemPool) {
    TransactionOutputParam param;
    //  param.format = TRANSACTION_DEFAULT_FORMAT;
    //  param.bandtype = TRANSACTION_DEFAULT_BANDTYPE;
    //  param.qulity = TRANSACTION_DEFAULT_QULITY;
    param.format = AUDIO_FORMAT_UNKNOWN;
    param.bandtype = UNKNOWN_BAND;
    param.qulity = 8;
    param.ssrc = ssrc;
    param.dtx = VL_FALSE;
    param.fec = VL_TRUE;
    param.cbr = VL_TRUE;
    param.extension = extension;
    param.extensionLen = extLen;
    param.extId = extId;
    initialOutgoing(remote, pEndPoint, savePath, param);
}

OutgoingTransaction::OutgoingTransaction(SockAddr& remote,
                                         shared_ptr<EndPoint> pEndPoint,
                                         MemoryPool* pktMemPool,
                                         const char* savePath,
                                         TransactionOutputParam& param)
    : pktMemPool(pktMemPool){
    initialOutgoing(remote, pEndPoint, savePath, param);
}

OutgoingTransaction::~OutgoingTransaction() {

    if(NULL != context.rtpExt) {
        free(context.rtpExt);
        context.rtpExt = NULL;
    }
    if(NULL != encoder) {
        delete encoder;
        encoder = NULL;
    }

    if(NULL != this->recordFilter) {
        delete recordFilter;
        recordFilter = NULL;
    }

    if(NULL != savePath) {
        free(savePath);
        savePath = NULL;
    }

    if(NULL != pcmConsummer) {
        delete pcmConsummer;
        pcmConsummer = NULL;
    }

    if(NULL != ioq) {
        if(NULL != refEndPoint) {
            //	  refEndPoint->unregisterSendQueue(ioq);
            EndPointManager::getInstance()->unregisterSendQueue(refEndPoint, ioq);
        }
        delete ioq;
        ioq = NULL;
    }
}

vl_bool OutgoingTransaction::sendEmptyRTP(vl_bool hasMark) {

    if(NULL != ioq && VL_TRUE == ioq->canWrite()) {

        int virlen = 0;
        if(VL_TRUE == hasMark) {
            virlen = 100;
        }

        /* 发送结束包 */
        vl_uint32 pktbuflen = context.maxPayloadSize + context.rtpAddonSize + virlen;
        vl_uint8 *pktbuf = (vl_uint8*) pktMemPool->allocate(pktbuflen);
        memset(pktbuf, 0, pktbuflen);
        RTPObject * rtpObj = new RTPObject(pktbuf, pktbuflen,  pktMemPool);

        /* 为不影响正常流程，发静音包，不发空包 */
        if(virlen != 0) {
            rtpObj->setPayloadType(PayloadTypeMapper::getPayloadType(encoder->getFormatId(), encoder->getSampleRate()));
        } else {
            rtpObj->setPayloadType(RTP_PT_UNKNOWN);
        }

        rtpObj->setSSRC(context.ssrc);
        rtpObj->setSequence(context.seq++);
        rtpObj->setPayloadByOffset(context.rtpPayloadOffset, virlen);
        /* 有附加数据 */
        if(0 != context.rtpExtNum && NULL != context.rtpExt) {
            rtpObj->attachExternalExtension(context.rtpExtId, context.rtpExtNum, context.rtpExt);
        }

        rtpObj->setTimestamp(getTimeStampSec());
        rtpObj->setMark(hasMark);
        rtpObj->setDecoded(VL_TRUE);

        Transmittable* transPtr = new Transmittable(EPROTO_RTP, &ioq->getRemoteAddr(), rtpObj);
        /* 进入ioq之后，生命周期由ioq管理 */
        if(VL_SUCCESS != ioq->write(transPtr)) {
            printf("transaction consume pcm, write to ioq failed");
            pktMemPool->release(pktbuf);
            delete rtpObj;
            delete transPtr;
        }
        return VL_TRUE;
    }
    return VL_FALSE;
}

static void* send_several_previous_packet(void* uc) {
    if(NULL != uc) {
        printf("OutgoingTransaction send previous packet ...");
        OutgoingTransaction* outtrans = (OutgoingTransaction*)uc;
        for(int i = 0; i < 5; i ++) {
            if(outtrans->sendEmptyRTP(VL_FALSE)) {
                Sleep(5);
            } else {
                break;
            }
        }
    }
    return NULL;
}

vl_status OutgoingTransaction::startTrans() {
    if(NULL == refEndPoint) {
        printf("transaction start failed, null endpoint");
        return VL_ERR_TRANS_PARAM;
    }

    if(NULL != ioq) {
        //	refEndPoint->registerSendQueue(ioq);
        EndPointManager::getInstance()->registerSendQueue(refEndPoint, ioq);
        /* 打开ioq输入输出 */
        ioq->openRead();
        pthread_create (&prevSendThread, NULL, send_several_previous_packet, this);
        srsSetRecordState(SRS_WAITING);
        return VL_SUCCESS;
    } else {
        printf("5555");
        return VL_ERR_TRANS_INIT;
    }
}

vl_status OutgoingTransaction::stopTrans(vl_uint32 msec) {
    //  stopRecrod();
    if(NULL != ioq) {
        /* 发送结束包 */
        printf("OutgoingTransaction send end packet");
        sendEmptyRTP(VL_TRUE);
        // sendEmptyRTP(VL_TRUE);
        // sendEmptyRTP(VL_TRUE);
        /* 设置ioq写入结束，等待 */
        ioq->closeWrite();
        /* 等待先发线程退出 */
        pthread_join(prevSendThread, NULL);

        if(VL_SUCCESS == ioq->asyncClose(msec)) {
            printf("OutgoingTransaction stop trasaction success");
            return VL_SUCCESS;
        }
        printf("OutgoingTransaction stop transaction asynchronize");
        return VL_ERR_IOQ_CLOSE_WAITING;
    }
    printf("OutgoingTransaction stop transaction failed");
    return VL_ERR_TRANS_PREPARE;
}

vl_status OutgoingTransaction::startRecord() {
    if(NULL != encoder) {
        AudioDevMgr* pAuddev = AudioDevMgr::getInstance();
        vl_status ret;

        do {
            RecordParameter recParam;
            /* 从编码器获取pcm参数 */
            encoder->getEncFramePCMInfo(&recParam.pcmInfo);
            /* 调整一次网络发送语音采样量 */
            //	  recParam.pcmInfo.sample_cnt  *= this->framesPerRTP;

            ret = pAuddev->aquireRecorder(recParam, pcmConsummer, &audioHandler);
            if(VL_SUCCESS != ret) {
                printf("transaction aquire recorder failed, ret=%d", ret);
                break;
            }
            ret = pAuddev->startRecord(audioHandler);
            if(VL_SUCCESS != ret) {
                printf("transaction start record failed, ret=%d", ret);
                break;
            }
            printf("DELAY DEBUG : start record done, ms=%lld", getTimeStampMS());
            srsSetRecordState(SRS_RECORING);
            /* 正常启动 */
            return VL_SUCCESS;
        } while(0);

        /* 出错 */
        if(AUDDEV_INVALID_ID != audioHandler) {
            pAuddev->releaseRecorder(audioHandler);
            audioHandler = AUDDEV_INVALID_ID;
        }

        return ret;
    } else {
        printf("transaction start record encoder is null");
        return VL_ERR_TRANS_INIT;
    }
}

vl_status OutgoingTransaction::stopRecrod() {
    srsSetRecordState(SRS_STOPING);

    if(AUDDEV_INVALID_ID != audioHandler) {
        AudioDevMgr* pAuddev = AudioDevMgr::getInstance();
        vl_status ret;

        ret = pAuddev->stopRecord(audioHandler);
        if(VL_SUCCESS != ret) {
            printf("transaction stop record failed : %d", ret);
        }

        /*
    ret = pAuddev->releaseRecorder(audioHandler);
    if(VL_SUCCESS != ret){
      printf("transaction release record failed : %d", ret);
    }
    audioHandler = AUDDEV_INVALID_ID;
    */
        return ret;
    }
    return VL_ERR_AUDDEV_ID;
}

vl_status OutgoingTransaction::releaseRecord() {
    if(AUDDEV_INVALID_ID != audioHandler) {
        AudioDevMgr* pAuddev = AudioDevMgr::getInstance();
        vl_status ret;
        ret = pAuddev->releaseRecorder(audioHandler);
        if(VL_SUCCESS != ret){
            printf("transaction release record failed : %d", ret);
        }
        audioHandler = AUDDEV_INVALID_ID;
        return ret;
    }
    return VL_ERR_AUDDEV_ID;
}

#if 0

vl_bool OutgoingTransaction::resetOutgoingBandType(ECODEC_BAND_TYPE bandType) {
  if(NULL == encoder || AUDDEV_INVALID_ID == audioHandler) {
    printf("transaction set out band type failed, encoder=%p, audioHandler=%d", encoder, audioHandler);
    return VL_FALSE;
  }

  if(encoder->getBandType() == bandType) {
    return VL_TRUE;
  } else {
    RecordParameter recParam;
    AudioDevMgr* pAuddev = AudioDevMgr::getInstance();

    encoder->resetBandType(bandType);
    encoder->getEncFramePCMInfo(&recParam.pcmInfo);
    /* 调整一次网络发送语音采样量 */
    //	recParam.pcmInfo.sample_cnt *= this->framesPerRTP;
    pAuddev->reconfigRecorder(recParam, audioHandler);
  }
}

vl_bool OutgoingTransaction::resetOutgoingQulity(vl_uint16 qulity){
  if(NULL != encoder) {
    encoder->setQulity(qulity);
    return VL_TRUE;
  } else {
    return VL_FALSE;
  }
}

#endif

vl_status OutgoingTransaction::srsStandby() {
    printf("OutgoingTransaction standby");
    return startTrans();
}

bool OutgoingTransaction::srsReadyForRecord() {
    if(SRS_WAITING == srsState) {
        printf("OutgoingTransaction query ready, true");
        return true;
    }
    printf("OutgoingTransaction query ready, false");
    return false;
}

vl_status OutgoingTransaction::srsStartRecord() {
   // printf("OutgoingTransaction srsStartRecord");
    return startRecord();
}

vl_status OutgoingTransaction::srsStopRecord(bool async) {
    //printf("OutgoingTransaction srsStopRecord, async=%d", async);
    return stopRecrod();
}

bool OutgoingTransaction::srsReadyForDispose(){
    if(SRS_STOPING == srsGetRecrodState() && VL_FALSE == AudioDevMgr::getInstance()->isRecording()) {
        printf("OutgoingTransaction is ready for dispose");
        return true;
    }
    printf("OutgoingTransaction not ready for dispose");
    return false;
}

bool OutgoingTransaction::srsReadyForDestroy() {
    vl_bool ready = readyForRelease();
    if(VL_TRUE == ready) {
        printf("OutgoingTransaction is ready for destroy");
        return true;
    } else {
        printf("OutgoingTransaction is not ready for destroy");
        return false;
    }
}

void OutgoingTransaction::srsDispose() {
    printf("OutgoingTransaction disposing");
    releaseRecord();
    srsSetRecordState(SRS_STOPED);
    if(VL_SUCCESS == stopTrans(200)) {
        StreamPlayScheduler::getInstance()->wakeup();
    }
}

SRS_STATE OutgoingTransaction::srsGetRecrodState() {
    return srsState;
}

void OutgoingTransaction::srsSetRecordState(SRS_STATE newState) {
    if(srsState == newState) {
        printf("OutgoingTransaction will set newState=%d, but current is the same", newState);
    } else if (newState < srsState) {
        printf("OutgoingTransaction srs update state downgrowth, old=%d, new=%d", srsState, newState);
        srsState = newState;
    } else {
        printf("OutgoingTransaction srs update from %d to %d", srsState, newState);
        srsState = newState;
    }
}

void OutgoingTransaction::onIOQueueUpdated() {
    //printf("OutgoingTransaction onIOQueue update ...");
    if(ioq != NULL) {
        if(EIOQ_CLOSED == ioq->getStatus() || EIOQ_CLOSE_TO == ioq->getStatus()) {
            printf("OutgoingTransaction and notify scheduler");
            StreamPlayScheduler::getInstance()->wakeup();
        }
    }
}

