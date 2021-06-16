
#include "SilkFileStreamSource.hpp"
#include "CodecManager.hpp"
#include "StreamPlayScheduler.hpp"



vl_status SilkFileStreamSource::spsStandby() {
  vl_status ret = VL_ERR_SILK_STORAGE_PREPARE_FAILED;

  if(VL_TRUE == silkFile.readable() && NULL != decoder && SPS_WAITING == playState) {
	ret = VL_SUCCESS;
  }
  return ret;
}

vl_status SilkFileStreamSource::spsStartTrack() {
  if(VL_TRUE == silkFile.readable() && NULL != decoder && SPS_WAITING == playState) {
	vl_status ret;
	AudioDevMgr* pAuddev = AudioDevMgr::getInstance();
	do {
	  PlayParameter plyParam;

	  decoder->getEncFramePCMInfo(&plyParam.pcmInfo);
      //plyParam.pcmInfo.sample_cnt *= 2;
      //plyParam.streamType = -1;
      plyParam.pcmInfo.sample_cnt = 160;
	  plyParam.auto_release = VL_FALSE;

	  ret = pAuddev->aquirePlayer(plyParam, this, &audioHandler, circleBuffer);
	  if(VL_SUCCESS != ret) {
        printf("Transaction aquire player failed, ret=%d", ret);
		break;
	  }

	  ret = pAuddev->startPlay(audioHandler);
	  if(VL_SUCCESS != ret) {
        printf("transaction start failed, ret=%d", ret);
		break;
	  }

	  playState = SPS_PLAYING;
	  return VL_SUCCESS;
	} while(0);

	if(AUDDEV_INVALID_ID != audioHandler) {
	  pAuddev->releasePlayer(audioHandler);
	  audioHandler = AUDDEV_INVALID_ID;
	}
    printf("SilkFileStreamSource start track failed, opensl operation return %d", ret);
	return ret;
  } else {
    printf("SilkFileStreamSource start track failed, initial failed : %d %p %d", silkFile.readable(), decoder, playState);
	return VL_ERR_SILK_STORAGE_CANT_PLAY;
  }
}

vl_status SilkFileStreamSource::spsPauseTrack() {
  return this->stopTrackInternal(VL_TRUE);
}

vl_status SilkFileStreamSource::spsResumeTrack() {
  return this->spsStartTrack();
}

void SilkFileStreamSource::spsMarkPlayed() {
  this->spsSetPlayState(SPS_PLAYED);
}

vl_status SilkFileStreamSource::stopTrackInternal(vl_bool isPause) {
  if(AUDDEV_INVALID_ID != audioHandler) {
	AudioDevMgr* pAuddev = AudioDevMgr::getInstance();
	vl_status ret;
	
	ret = pAuddev->stopPlay(audioHandler, VL_FALSE);
	if(VL_SUCCESS != ret) {
      printf("SilkFileStreamSource play failed : %d", ret);
	}
	ret = pAuddev->releasePlayer(audioHandler);
	if(VL_SUCCESS != ret){
      printf("SilkFileStreamSource release player failed : %d", ret);
	}
	audioHandler = AUDDEV_INVALID_ID;


	if(VL_FALSE == isPause) {
	  playState = SPS_PLAYED;
	} else {
	  playState = SPS_PAUSED;
	}
  }
  return 0;
}

vl_status SilkFileStreamSource::spsStopTrack() {
  return this->stopTrackInternal();
}

vl_status SilkFileStreamSource::spsDispose() {
  return VL_SUCCESS;
}

vl_bool SilkFileStreamSource::spsReadyForPlay() const { 
  if(VL_TRUE == silkFile.readable() && NULL != decoder && SPS_WAITING == playState) {
	return VL_TRUE;
  }
  return VL_FALSE;
}

vl_bool SilkFileStreamSource::spsReadyForDestroy() {
  return SPS_PLAYED == playState;
}

vl_uint32 SilkFileStreamSource::spsGetIdentify() const {
  return identity;
}

SPS_STATE SilkFileStreamSource::spsGetPlayState() const {
  return playState;
}

void SilkFileStreamSource::spsSetPlayState(SPS_STATE newState) {
  this->playState = newState;
}

void SilkFileStreamSource::spsGetResultInfo(struct SpsResultInfo& info) const {
  
}

/*
vl_status SilkFileStreamSource::feedPcm(PCMInfo* pcmInfo, UnitCircleBuffer* circleBuffer) {
  if(VL_TRUE == circleBuffer->hasMoreData()) {
    // 确保circlebuffer有足够的剩余空间
	if(circleBuffer->getFreeSize() > (circleBuffer->getCapacity() / 2)) {
	  SilkBlock block;
	  if(VL_SUCCESS == silkFile.readBlock(block)) {
		vl_uint8* payload = (vl_uint8*)block.payload;
		vl_size payloadlen = block.length;

		if(NULL == payload || payloadlen <= 0) {
          printf("SilkFileStreamSource no data to feed from block payload = %p payloadlen = %d", payload, payloadlen);
		  return VL_SUCCESS;
		}

		AudPacket * encPkt = new AudPacket((vl_int8*)payload, payloadlen, 0, NULL, VL_FALSE);
		encPkt->setFormat(silkFile.getFormat());
		encPkt->setTimestamp(block.timestamp);

		vl_status ret = decoder->decode(encPkt, circleBuffer);

		if(VL_SUCCESS != ret) {
          printf("SilkFileStreamSource decode failed ret = %d payload size = %d",ret, payloadlen);
		}
	  } else {
		circleBuffer->closeWrite();
	  }
	}
  }

  if(VL_TRUE == circleBuffer->isEmpty() && VL_FALSE == circleBuffer->hasMoreData()) {
	playState = SPS_NO_MORE;
	notifyScheduler();
  }
  return VL_SUCCESS;
}*/


vl_status SilkFileStreamSource::feedPcm(PCMInfo* pcmInfo, UnitCircleBuffer* circleBuffer) {
    if(VL_TRUE == circleBuffer->hasMoreData()) {
        /* 确保circlebuffer有足够的剩余空间 */
        //jams
        do {
            if(circleBuffer->getFreeSize() > (circleBuffer->getCapacity() / 2)) {
                SilkBlock block;
                if(VL_SUCCESS == silkFile.readBlock(block)) {
                    vl_uint8* payload = (vl_uint8*)block.payload;
                    vl_size payloadlen = block.length;

                    if(NULL == payload || payloadlen <= 0) {
                        printf("SilkFileStreamSource no data to feed from block payload = %p payloadlen = %d", payload, payloadlen);
                        return VL_SUCCESS;
                    }

                    AudPacket * encPkt = new AudPacket((vl_int8*)payload, payloadlen, 0, NULL, VL_FALSE);
                    encPkt->setFormat(silkFile.getFormat());
                    encPkt->setTimestamp(block.timestamp);

                    vl_status ret = decoder->decode(encPkt, circleBuffer);

                    if(VL_SUCCESS != ret) {
                        printf("SilkFileStreamSource decode failed ret = %d payload size = %d",ret, payloadlen);
                    }
                } else {
                    circleBuffer->closeWrite();
                    break;
                }
            }
        }while(circleBuffer->getCapacity() - circleBuffer->getLength() > 16000);
    }

    if(VL_TRUE == circleBuffer->isEmpty() && VL_FALSE == circleBuffer->hasMoreData()) {
        playState = SPS_NO_MORE;
        notifyScheduler();
    }
    return VL_SUCCESS;
}

SilkFileStreamSource::~SilkFileStreamSource() {
  if(NULL != circleBuffer) {
	delete circleBuffer;
	circleBuffer = NULL;
  }
}

SilkFileStreamSource::SilkFileStreamSource(const char* filePath, vl_uint32 identity) :
  silkFile(filePath, VL_FALSE),
  identity(identity),
  decoder(NULL),
  playState(SPS_INVALID),
  circleBuffer(NULL) {

  if(VL_TRUE == silkFile.readable()) {
	AudioDecoderParam decodeParam;
	decodeParam.plc = VL_FALSE;
 
	EAUDIO_FORMAT format = silkFile.getFormat();
    printf("SilkFileStreamSource , file format is %d ", format);

	SilkBlock block;
	if(VL_SUCCESS == silkFile.readBlock(block)) {

	  vl_bool sprMatched = VL_TRUE;
	  switch(block.spr) {
	  case ESILK_BLOCK_8K:
		decodeParam.sampleRate = 8000;
		break;
	  case ESILK_BLOCK_12K:
		decodeParam.sampleRate = 12000;
		break;
	  case ESILK_BLOCK_16K:
		decodeParam.sampleRate = 16000;
		break;
	  case ESILK_BLOCK_24K:
		decodeParam.sampleRate = 24000;
		break;
	  default:
        printf("SilkFileStreamSource bandtype is not matched !!!!!!!!!!");
		sprMatched = VL_FALSE;
		break;
	  }

	  if(VL_TRUE == sprMatched) {
		//		decoder = CodecManager::newAudioDecoder(AUDIO_FORMAT_SILK, decodeParam);
		decoder = CodecManager::createAudioDecoder(format, decodeParam);
	  }

	  if(NULL != decoder) {
		//		LOGI("SilkFileStreamSource construct success , bandtype is %d ", decodeParam.bandType);
		silkFile.resetOffset();
		playState = SPS_WAITING;
	  }
	  
	  if(NULL == circleBuffer) {
		circleBuffer = new UnitCircleBuffer();
	  }
	}
  } else {
    printf("SilkFileStreamSource construct initial failed, file cant read !!!");
  }
}

void SilkFileStreamSource::notifyScheduler() {
  StreamPlayScheduler::getInstance()->wakeup();
}



