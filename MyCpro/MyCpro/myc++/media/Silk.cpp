#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "vl_types.h"
#include "vl_const.h"
#include "Silk.hpp"
#include "CodecManager.hpp"
#include "AudioCodec.hpp"

#define __MALLOC   malloc
#define __FREE     free
#define __ASSERT   assert

const char* SilkEncoder::name = "silk";

vl_uint8 SilkEncoder::bpSample[1] = {16};

#define MAX_FRAME_PER_PACKET  (5)

vl_status SilkEncoder::updateParam() {
    if(0 == pthread_mutex_trylock(&stateMutex)) {
        encControl.API_sampleRate = getSampleRate();
        encControl.bitRate = getBitrate();
        encControl.packetLossPercentage = SILK_ENC_CTL_PACKET_LOSS_PCT;
        encControl.complexity = 2;
        encControl.maxInternalSampleRate = encControl.API_sampleRate;
        pthread_mutex_unlock(&stateMutex);
        return VL_SUCCESS;
    } else {
        return VL_FAILED;
    }
}

SilkEncoder::SilkEncoder(const AudioEncoderParam& encParam) :
    AudioEncoder(AUDIO_FORMAT_SILK, encParam), encState(NULL)
{
    SKP_int st_size, err;

    memset(&encControl, 0, sizeof(SKP_SILK_SDK_EncControlStruct));

    err = SKP_Silk_SDK_Get_Encoder_Size(&st_size);
    if(0 != err) {
        printf("silk get encode status size error, errno = %d !!", err);
        return;
    }

    encState = __MALLOC(st_size);
    if(NULL == encState) {
        printf("silk alloc encoder state failed");
        return;
    }
    memset(encState, 0, st_size);

    err = SKP_Silk_SDK_InitEncoder(encState, &encControl);
    if(0 != err) {
        printf("silk initial encoder error, errno = %d !!", err);
        return;
    }
    printf("SilkEncoder::SilkEncoder=========");
    updateParam();
    enabled = VL_TRUE;
}

SilkEncoder::~SilkEncoder() {
    if(NULL != encState) {
        __FREE(encState);
        encState = NULL;
    }
}

vl_bool SilkEncoder::setDTX(vl_bool enable) {
    encControl.useDTX = (VL_TRUE == enable) ? 1 : 0;
    printf("set dtx %d", encControl.useDTX);
    enable_DTX = enable;
    return enable_DTX;
}

vl_bool SilkEncoder::setFEC(vl_bool enable) {
    encControl.useInBandFEC = (VL_TRUE == enable) ? 1 : 0;
    printf("set fec %d", encControl.useInBandFEC);
    enable_FEC = enable;
    return enable_FEC;
}

vl_size SilkEncoder::getMaxPayloadSize() const {
    /*
   * assume bitrate of 40kpbs (5bytes / ms)
   */
    vl_size scpms = sampleRate / 1000;
    vl_size durMs = samplePerPackage / scpms;
    if(samplePerPackage % scpms != 0) {
        durMs ++;
    }
    vl_size frmPerRTP = durMs / getFrameMS();
    if(durMs % getFrameMS() != 0) {
        frmPerRTP ++;
    }

    return (5 * durMs + AudPacket::getMaxBlockPaddingSize()) * frmPerRTP;
}

vl_size SilkEncoder::getMaxPayloadSizePerFrame() const {
    /*
   * assume bitrate of 40kpbs (5bytes / ms)
   */
    return 5 * getFrameMS();
}

vl_status SilkEncoder::getEncodedPacket(AudPacket* output)
{
    vl_status ret = VL_SUCCESS;
    vl_size sampleCount = samplePerPackage;
    vl_uint16 frames;
    SKP_int result;
    vl_size samplePerFrame;
    SKP_int16* ptrSample = NULL;


    if(NULL == encState || VL_TRUE != enabled) {
        printf("silk encode error, encoder is not ready");
        return VL_ERR_CODEC_ENC_ERROR;
    }

    if(0 != pthread_mutex_trylock(&stateMutex) ) {
        printf("silk encode error, encoder is occupied");
        return VL_ERR_CODEC_ENC_ERROR;
    }

    if(sampleCount > circleBuffer->getLength() && VL_FALSE == circleBuffer->hasMoreData())  {
        sampleCount = circleBuffer->getLength();
    }

    /* specify sample count per audio packet */
    if(VL_SUCCESS != circleBuffer->getRange((vl_int16**)&ptrSample, sampleCount)) {
        return VL_ERR_CODEC_ENC_NOT_ENOUGH;
    }

    samplePerFrame = getSampleRate() * getFrameMS() / 1000;

    frames = sampleCount / samplePerFrame;
    if(0 != sampleCount % samplePerFrame) {
        frames ++;
        printf("silk encode ,input pcm duration is no match packge restrict");
    }

    int maxSamplePerPacket = samplePerFrame * getMaxFramesPerPacket();
    int encodedSamples = 0;
    while(encodedSamples < sampleCount)
    {
        int leftSamples = sampleCount - encodedSamples;
        int currEncSampleCount = (maxSamplePerPacket < leftSamples) ? maxSamplePerPacket : leftSamples;
        int currFrames = currEncSampleCount / samplePerFrame;
        if(currEncSampleCount % samplePerFrame != 0) {
            currFrames ++;
        }

        encControl.packetSize = currEncSampleCount;

        /* 从output中请求缓冲 */
        vl_int8* encPtr = NULL;
        int encBufferSize = getMaxPayloadSizePerFrame() * currFrames;

        SKP_int16 outSize;
        if(output->getLeftRange(&encPtr, encBufferSize) > 0)
        {
            //printf("silk encode one");
            outSize = encBufferSize;
            result = SKP_Silk_SDK_Encode(encState,
                                         &encControl,
                                         ptrSample+encodedSamples,
                                         (SKP_int) encControl.packetSize,
                                         (SKP_uint8*)encPtr,
                                         &outSize);

            //printf("Silk calculate encBuffer size = %d, packetsize=%d, frames=%d, sampleCount = %d", encBufferSize,encControl.packetSize, currFrames, sampleCount);
            if(0 == result) {
                output->fixBlockSize(outSize);
            } else {
                /* 编码失败 */
                printf("Silk encode failed, errno = %d", result);
                ret = VL_ERR_CODEC_ENC_ERROR;
                break;
            }
        } else {
            printf("silk encode, output packet has no enough buffer size=%d",encBufferSize);
            break;
        }

        /* 记录已编码的声音样本数 */
        encodedSamples += currEncSampleCount;
    }

    if(VL_FALSE == output->closePacket()) {
        printf("silk close output packet failed");
        ret = VL_ERR_CODEC_ENC_ERROR;
    }

    pthread_mutex_unlock(&stateMutex);
    return ret;

}

vl_status SilkEncoder::getEncFramePCMInfo(PCMInfo * info) const {
    if(VL_TRUE != this->enabled) {
        return VL_FAILED;
    }
    info->sample_cnt = getSampleRate() * getFrameMS()  / 1000;
    info->sample_rate = getSampleRate();
    info->channel_cnt = 1;
    info->bits_per_sample = getBytesPerSample() << 3;
    return VL_SUCCESS;
}

SilkDecoder::SilkDecoder(const AudioDecoderParam& param) :
    AudioDecoder(AUDIO_FORMAT_SILK, param), decState(NULL) {
    SKP_int st_size, err;

    memset(&decControl, 0, sizeof(SKP_SILK_SDK_DecControlStruct));

    /* 初始化解码器 */
    err = SKP_Silk_SDK_Get_Decoder_Size(&st_size);
    if(err) {
        printf("silk get decode3 status size error, errno = %d !!", err);
        return;
    }
    decState = __MALLOC(st_size);
    err = SKP_Silk_SDK_InitDecoder(decState);
    if(err) {
        printf("silk initial decoder error, errno = %d!", err);
        return;
    }
    updateParam();

    enabled = VL_TRUE;
}

SilkDecoder::~SilkDecoder() {
    if(NULL != decState) {
        __FREE(decState);
        decState = NULL;
    }
}

vl_uint32 SilkDecoder::convertSampleRate(ECODEC_BAND_TYPE bandType) const {
    //   return ::silkConvertSampleRate(bandType);

    return PayloadTypeMapper::getSampleRateByBandtype(bandType);
}

vl_bool SilkDecoder::setPLC(vl_bool enable) {
    enable_plc = enable;
    return this->enable_plc;
}

vl_status SilkDecoder::updateParam() {
    if(0 == pthread_mutex_trylock(&stateMutex)) {
        decControl.API_sampleRate = getSampleRate();
        pthread_mutex_unlock(&stateMutex);
        return VL_SUCCESS;
    } else {
        return VL_FAILED;
    }
}

vl_status SilkDecoder::decode(vl_int8* packet, vl_size size, UnitCircleBuffer* circleBuffer) {
    SKP_int result;

    do {
        SKP_int16 frameLen;
        vl_size samplePerFrame = getSampleRate() * getFrameMS() / 1000;
        SKP_int16* sampleOutPut = (SKP_int16*)circleBuffer->markRange(samplePerFrame);

        if(NULL == sampleOutPut) {
            printf("silk decode failed, not enough memory in circle buffer");
            result = -1;
            break;
        }

        result = SKP_Silk_SDK_Decode(decState,
                                     &decControl,
                                     0,
                                     (const SKP_uint8 *) packet,
                                     (const SKP_int) size,
                                     sampleOutPut,
                                     &frameLen);

        if(SKP_SILK_NO_ERROR != result) {
            printf("SilkDecoder decode failed, errno = %d ", result);
            break;
        }

        if(frameLen != samplePerFrame) {
            printf("silk decode warring, get samples not in frame");
        }

    } while( decControl.moreInternalDecoderFrames );

    if(SKP_SILK_NO_ERROR != result) {
        return VL_ERR_CODEC_DEC_ERROR;
    } else {
        return VL_SUCCESS;
    }
}

/**
  * 解码pcm，需保证该函数可重入和线程安全。
  * 解码时，将一个包解析出来的帧数组逐个调用decode进行解码，也就是说，每次解码只会解码一帧数据
  * input : [in] 编码帧信息
  * output : [out] 解码出来的帧信息
  */
vl_status SilkDecoder::decode(AudPacket* input, UnitCircleBuffer* circleBuffer) {
    SKP_int result;
    vl_status ret;

    if(NULL == input || NULL == circleBuffer) {
        printf("silk decode error, input or sample is null");
        return VL_ERR_CODEC_DEC_ERROR;
    }

    if(NULL == decState || VL_TRUE != enabled) {
        printf("silk decode error, decState is null or not enabled");
        return VL_ERR_CODEC_DEC_ERROR;
    }

    if(input->getFormat() != this->formatId) {
        printf("silk decode, audio format unmatched");
        return VL_ERR_CODEC_DEC_ERROR;
    }

    if(0 != pthread_mutex_trylock(&stateMutex) ) {
        printf("silk deocde error, decoder is occupied");
        return VL_ERR_CODEC_ENC_ERROR;
    }

    /* silk sample为16bit */

    vl_int8 * ptr;
    vl_size blockSize;

    if(VL_TRUE == input->isBlocked()) {
        while ((blockSize = input->getNextBlock(&ptr)) > 0) {
            ret = decode(ptr, blockSize, circleBuffer);
            if(ret != VL_SUCCESS) {
                printf("silk decode error : %d, size=%d", result, input->getCapacity());
                break;
            }
        }
    } else {
        ret = decode(input->getBuffer(), input->getCapacity(), circleBuffer);
    }

    pthread_mutex_unlock(&stateMutex);
    return ret;
}

vl_status SilkDecoder::getEncFramePCMInfo(PCMInfo *info) const {
    if(VL_TRUE != this->enabled) {
        return VL_FAILED;
    }
    info->sample_cnt = getSampleRate() * getFrameMS() / 1000;
    info->sample_rate = getSampleRate();
    info->channel_cnt = 1;
    info->bits_per_sample = 16;
    return VL_SUCCESS;
}
