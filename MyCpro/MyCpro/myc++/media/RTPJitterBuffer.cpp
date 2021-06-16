#include "RTPJitterBuffer.hpp" 
#include "RTPRecvIOQueue.hpp"
#include "AudioInfo.hpp"
#include "CodecManager.hpp"

inline vl_int32 getAbs(vl_int32 number) {
  return number > 0 ? number : (0 - number);
}

void RTPJitterBuffer::popupJitter() {

  RTPObject* rtpObj = NULL;

  pthread_mutex_lock(&jitterLock); 
  
  if(buffer.size() > dqThreshold || isEnded == VL_TRUE) {	
	if(dqThreshold > 0) {
	  resetThreshold();
	}

	while(buffer.size() > 0) {
      //printf("[MUTEX] LOCK %s %p",__FUNCTION__,&jitterLock);

	  Transmittable* deTrans = buffer.top();
	  rtpObj = dynamic_cast<RTPObject*>(deTrans->getObject());
	  owner->IOQueue::write(deTrans);
	  buffer.pop();
      //printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&jitterLock);
	}
  }

  pthread_mutex_unlock(&jitterLock);
}


void RTPJitterBuffer::recalculateThreshold(vl_bool force) {
  /*
   * pc < 5 : 不用预测
   * pc > 5 && pc < 20 : 预测，threshold >= min    NOTE : 为了缩短延迟，暂时不用这个
   * pc >= 20 : threshold > 0
   */
  
  if(totalRTPCount > 0) {
	if(VL_TRUE == force || dqThreshold != 0) {
	  if(totalRTPCount < minThreshold && dqThreshold != minThreshold) {
		resetThreshold(minThreshold);
	  } else if(rtpDuration > 0) {
		vl_int32 estimateJitter = totalJitter / totalRTPCount;
		vl_size tmpThres = 0;
		vl_bool isTimeout = VL_FALSE;

		if(owner != NULL && (EIOQ_WRITE_TIMEOUT_WAITING == owner->getWriteStatus() || EIOQ_WRITE_TIMEOUT == owner->getWriteStatus())) {
		  isTimeout = VL_TRUE;
		} 

		/* if has receive end or timeout, do not cache */
		if(isEnded != VL_TRUE && isTimeout != VL_TRUE) {
		  /* 起始包容易丢弃，前20个包时不做计算。
		  if(totalRTPCount > N_PREDICT_DEPENDS_PACKEETS && getLostPercent() > 0) {
			tmpThres += (N_EXPECT_CONTINOUS * getLostPercent() / 100) / rtpDuration;
		  }
		  */

		  if(estimateJitter > 20) {
		  
			tmpThres += (estimateJitter * N_EXPECT_CONTINOUS) / ((rtpDuration + estimateJitter) * rtpDuration);
            printf("RTPJitterBuffer recalculate threshold=%d, totalRTP=%d, totalJitter=%d rtpDuration=%d, factor=%d",
				 tmpThres, totalRTPCount, totalJitter, rtpDuration, tmpThres);

			if(tmpThres > 10) {
			  tmpThres = 10;
			}

			/* 四舍五入 */
			/*
			  vl_size factor = ((estimateJitter * N_EXPECT_CONTINOUS) << 1) / ((rtpDuration + estimateJitter) * rtpDuration);

			  tmpThres += factor / 2;
			  if(factor >= 1) {
			  tmpThres ++;
			  }
			*/
		  }
		}
		resetThreshold(tmpThres);
	  }
	}
  }
}

void RTPJitterBuffer::rehold() {
  /* 发现中断，调整dqThreshold */
  if(dqThreshold == 0) {
	//	LOGI("rtp-jitter : totalJitter = %d , maxjitter = %d  lostPercent = %d", totalJitter,  maxJitter, lostPercent);
	recalculateThreshold(VL_TRUE);
  }
}

void RTPJitterBuffer::push(Transmittable* trans) {
  
  if(NULL == trans) {
    printf("jitter buffer push null");
	return;
  }
  if(EPROTO_RTP != trans->getProtocol()) {
    printf("jitter buffer push unknown protocol");
	return;
  }

  RTPObject* rtpObj = dynamic_cast<RTPObject*>(trans->getObject());
  if(NULL == rtpObj) {
    printf("RTPJitterBuffer parse a transamittable thant not contain object");
	delete trans;
	return;
  }

  owner->setDirty();

  if(VL_TRUE != rtpObj->isDecoded()) { 
    printf("RTPJitterBuffer parse and rtp object that is not decoded");
	return; 
  }

  vl_uint64 currRTPTS = getTimeStampMS();
  
  /* 前置包可能没有可用语音包 */
  if(0 == rtpDuration && rtpObj->getPayloadType() != RTP_VIDEO_PAYTYPE && rtpObj->getPayloadType() != RTP_MEDIACODEC_PAYTYPE) {
	rtpDuration = parseDuration(rtpObj);
  }

  if(totalRTPCount > 0 && lastSendTimestamp != 0) {
	vl_int32 currJitter = (vl_uint32)(currRTPTS - lastRecvTimestamp) - (rtpObj->getTimestamp() - lastSendTimestamp);
	if(getAbs(currJitter) > getAbs(maxJitter)) {
	  maxJitter = currJitter;
	}
	totalJitter += currJitter;
  }

  if(rtpObj->getSequenceNumber() > lastSequence) {
	lastSequence = rtpObj->getSequenceNumber();
  }

  lastSendTimestamp = rtpObj->getTimestamp();
  lastRecvTimestamp = currRTPTS; 

  totalRTPCount ++;
  /* 将语音推入抖动缓冲 */

  //  LOGI("RTPJitterBuffer payload len = %d, payload = %p", rtpObj->getPayloadLength(), rtpObj->getPayload());
  
  if(rtpObj->getPayloadLength() > 0 || rtpObj->hasMarker()) {
    //printf("[MUTEX] LOCK %s %p",__FUNCTION__,&jitterLock);
	pthread_mutex_lock(&jitterLock);
	buffer.push(trans);
	pthread_mutex_unlock(&jitterLock);
    //printf("[MUTEX] UNLOCK %s %p",__FUNCTION__,&jitterLock);
  } else {
    if(rtpObj->getSequenceNumber() > 5)
      printf("RTPJitterBuffer seq=%d receive rtp has no payload", rtpObj->getSequenceNumber());
  }

  if(rtpObj->hasMarker()) {
	//LOGI("RTPJitterBuffer receive last packet");
	isEnded = VL_TRUE;
  }

  /* 缓冲过程，大量数据计算增大准确性，重新计算抖动大小 */
  recalculateThreshold();

  /* popup jitterbuffer */
  popupJitter();

  /* 更新接收ioq数据 */
  owner->onJitterReceive();
}


vl_int32 RTPJitterBuffer::getPredictContinousMS() const {
  vl_int32 predictMS = 0;
  vl_size leftRTPSize = owner->getBufferdSize() + buffer.size();
  vl_bool isTimeout = VL_FALSE;

  if(0 == rtpDuration) {
	return 0;
  }

  if(owner != NULL && (EIOQ_WRITE_TIMEOUT_WAITING == owner->getWriteStatus() || EIOQ_WRITE_TIMEOUT == owner->getWriteStatus())) {
	isTimeout = VL_TRUE;
  }

  if((VL_TRUE == isEnded || VL_TRUE == isTimeout) && leftRTPSize > 0) {
	/* has receive a last packet or timeout and there has valid audio package, judge it's ready to play */
	predictMS = 60 * 60 * 1000;
  } else if(totalRTPCount >= minThreshold && totalRTPCount > 0 && leftRTPSize > 0 && rtpDuration > 0) {
	/* 暂时用平均抖动算 */
	vl_int32 estimateJitter = totalJitter / totalRTPCount;

	if(estimateJitter <= 0) {
	  /* 粗略估计一个较大的值 1小时，因为平均抖动为负数 */
	  predictMS = 60 * 60 * 1000;
      printf("RTPJitterBuffer predict ms=%d, totalJitter=%d", predictMS, totalJitter);
	} else {
	  predictMS = leftRTPSize * rtpDuration * (rtpDuration + estimateJitter) / estimateJitter;
      printf("RTPJitterBuffer predict ms=%d,buffer size=%d, ioqsize=%d, total jitter=%d, totalcount=%d",
		   predictMS, buffer.size(),owner->getBufferdSize(), totalJitter, totalRTPCount);
	}
  } else {
    printf("RTPJitterBuffer predict ms=%d, totalRTPCount=%d, minThreshold=%d, buffer size=%d, rtpDur=%d", predictMS, totalRTPCount, minThreshold, leftRTPSize, rtpDuration);
	//LOGI("RTPJitterBuffer predict 0 totalcout=%d", totalRTPCount);
  }

  return predictMS;
}

vl_int32 RTPJitterBuffer::getTotalJitter() const {
  if(totalRTPCount > 2) {
	return totalJitter;
  } else {
	return INT_MAX;
  }
}


vl_int32 RTPJitterBuffer::parseDuration(RTPObject* rtpObj) {
  vl_int32 msecPerRTP = 0;
  EAUDIO_FORMAT format = PayloadTypeMapper::getCodecByPT((ERTP_PT)rtpObj->getPayloadType());
  ECODEC_BAND_TYPE bandType = PayloadTypeMapper::getBandTypeByPT((ERTP_PT)rtpObj->getPayloadType()); 

  // CodecManager::getCodecByPT((ERTP_PT)rtpObj->getPayloadType(), &format, &bandType);

  /* 现只支持silk编码的语音帧 */
  if(format == AUDIO_FORMAT_SILK) {
	msecPerRTP = 200;

  }else{
    if(rtpObj->getSequenceNumber() > 5) {
      printf("RTPJitter seq=%d not evrc or silk!!!", rtpObj->getSequenceNumber());
    }
  }

  return msecPerRTP;
}

void RTPJitterBuffer::resetThreshold(vl_size newThreshold) {
  //LOGW("RTPJitterBuffer threshold update to %d ", newThreshold);
  dqThreshold = newThreshold;
}

vl_uint32 RTPJitterBuffer::getLostPercent() const {
  vl_uint32 lostPercent = 0;
  if(lastSequence != 0) {
	lostPercent = (lastSequence + 1 - totalRTPCount) * 100 / (lastSequence + 1);
    printf("RTPJitterBuffer calculate lost persend get seq = %d rtptotal = %d lost = %d ", lastSequence, totalRTPCount, lostPercent);
  }

  return lostPercent;
}
 
