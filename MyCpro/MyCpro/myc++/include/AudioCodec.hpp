#ifndef __AUDIO_CODEC_HPP__
#define __AUDIO_CODEC_HPP__

#include <stdlib.h>
#include <string>

#include "pthread.h"
#include "vl_types.h"
#include "vl_const.h"
#include "AudioInfo.hpp"
#include "UnitCircleBuffer.hpp"

// base on UnitCircleBuffer's characteristic, use mark to read data, capacity must multiple
// with factor samplePerPackage. currently use factor 4
#define ENCODER_CACHE_FACTOR    (4)

/**
 * AudioCodec just can create by CodecManager.
 */
class CodecManager;

/**
 * 考虑网络各种适应情况，编解码器和声音设备需要动态调整。如mtu大小，网络差等，会影响采样率，样本大小，一个包所包含的帧个数。
 * 注意： 由于编码解码需要上下文，因此编码和解码过程不可重入，多线程环境需创建多个编解码器实例。
 * 编码器可配置的包括：采样率，比特率，帧数来。采样率和比特率分别影响带宽消耗，帧数影响每一次发包大小。
 */

/**
 * 封装包概念
 * package(rtp payload) -> packet(refer to codec packet) -> frame(refer to codec frame)
 */

/*
#define TRANSACTION_DEFAULT_BANDTYPE  (WIDE_BAND)
#define TRANSACTION_DEFAULT_QULITY    (8)
#define TRANSACTION_DEFAULT_FORMAT    (AUDIO_FORMAT_SILK)
*/

typedef struct {
  vl_uint32 sampleRate;   // sample rate for input pcm
  vl_int32 bitrate;       // bitrate for output
  vl_uint32 packageSampleCount; // sample count per package
  /**
   * 编解码动态配置参数，默认编码器实现不支持
   */ 
  vl_bool dtx;            // 是否支持不连续传输
  vl_bool cbr;            // 保持常量比特率
  vl_bool fec;            // 帧补偿
} AudioEncoderParam;

typedef struct {
  //  ECODEC_BAND_TYPE bandType;
  vl_uint32 sampleRate;
  /**
   * 编解码动态配置参数，默认解码器实现不支持
   */ 
  vl_bool plc;            // 丢帧补全
} AudioDecoderParam;


/**
 * 编码器接口
 */
class AudioEncoder {
  friend CodecManager;
public:
  /* 
   *检测编码器是否准备好进行编码。
    * @返回VL_TRUE（如果已准备就绪），否则返回VL_FALSE
   */
  vl_bool isEnable() const { return enabled; };

  /**
   * 获取用于存储rtp数据包有效负载的最大有效负载大小。
    * RTP有效负载指示AudioPacket需要存储的分配内存大小。
    * @返回最大有效负载大小。
   */
  virtual vl_size getMaxPayloadSize() const = 0;

  /**
   * PCM数据将首先缓存到循环缓冲区中，但编码器不应立即进行编码。
    *将pcm数据排入缓存后，可以调用@readyToGetPacket以检查是否有时间获取
    *使用循环程序编码的数据包，并调用＆getEncodedPacket以获取数据包
    * @return <0表示错误otherwize表示多少
   */
  virtual vl_bool enqueuePCM(const void* samples, vl_size bytes) {
	if(VL_TRUE != enabled) {
      printf("audio encoder enqueme pcm when not enable");
	  return VL_FALSE;
	}
	return (VL_SUCCESS == circleBuffer->write((vl_int16*) samples, bytes / 2)) ? VL_TRUE : VL_FALSE;
  }
  
  /**
   * 指示编码的数据包是否已准备好读取。
    * @如果返回VL_FALSE，则返回VL_TREU。
   */
  virtual vl_bool readyToGetPacket() {
  	if(VL_TRUE != enabled) {
		return VL_FALSE;
  	}
	if(circleBuffer->getLength() >= samplePerPackage) {
	  return VL_TRUE;
	} else {
	  return VL_FALSE;
	}
  }
  
  /**
   * 用于获取编码的音频数据包的接口。
    * @return如果有有效的数据包要获取，则返回VL_TRUE。
   */
  virtual vl_status getEncodedPacket(AudPacket* output) = 0;

  /**
   * 即使没有足够的pcm数据进行编码，也要强制编码器对PCM进行编码。
    *当没有更多pcm数据时，请使用此功能。
   */
  void toggleForceEncode() { 
	if(NULL != circleBuffer) {
	  circleBuffer->closeWrite();
	}
  }

  /* 
   * AudioEncoder构造函数，创建循环缓冲区并初始化线程锁。
   */
  AudioEncoder(EAUDIO_FORMAT formatId, const AudioEncoderParam& encParam) : 
	enabled(VL_FALSE), formatId(formatId), circleBuffer(NULL) {
    /* 必要的配置 */
	setBitrate(encParam.bitrate);
	setSampleRate(encParam.sampleRate);
	setBytesPerSample();
	setSampleCountPerPackage(encParam.packageSampleCount);
	
	/* 具体编码器若支持，将在子类构造中重设 */
	setDTX(encParam.dtx);
	setCBR(encParam.cbr);
    setFEC(encParam.fec);

	circleBuffer = new UnitCircleBuffer(ENCODER_CACHE_FACTOR * this->samplePerPackage);
	pthread_mutex_init(&stateMutex, NULL);

	if(NULL == circleBuffer) {
      printf("audio encoder alloc circle buffer failed");
	}
  }

  virtual ~AudioEncoder() {
	if(NULL != circleBuffer) {
	  delete circleBuffer;
	  circleBuffer = NULL;
	}
	pthread_mutex_destroy(&stateMutex);
  }

  /**
   * 为特定带宽类型编码设置pcm信息，该信息可用于录音参数设置。
   * 确定采样率，一帧语音sample数量
   * 声道数被设置为1，单个sample占用两个字节
   */
  virtual vl_status getEncFramePCMInfo(PCMInfo * info) const{
    if(VL_TRUE != this->enabled) {
		return VL_FAILED;
  	}
	/* aspect every 20ms to get new pcm */
  	info->sample_cnt = getSampleRate() * 20  / 1000;
  	info->sample_rate = getSampleRate();
  	info->channel_cnt = 1;
  	info->bits_per_sample = getBytesPerSample() << 3;
  	return VL_SUCCESS;
  }

  /* 返回是否打开成功 */
  vl_bool hasDTX() const { return enable_DTX; }
  vl_bool hasCBR() const { return enable_CBR; }
  vl_bool hasFEC() const { return enable_FEC; }


  /* 具体编码器若支持该功能，需重写接口 */
  virtual vl_bool setDTX(vl_bool enable) { 
	enable_DTX = VL_FALSE; 
	return enable_DTX;
  }
  virtual vl_bool setCBR(vl_bool enable) { 
	enable_CBR = VL_FALSE; 
	return enable_CBR;
  }
  virtual vl_bool setFEC(vl_bool enable) { 
	enable_FEC = VL_FALSE; 
	return enable_FEC;
  }

  /**
   * 多少样本将编码并打包到rtp数据包的配置。
   */
  void setSampleCountPerPackage(vl_uint32 sampleCount) { this->samplePerPackage = sampleCount; }
  vl_uint32 getSampleCountPerPackage() const { return this->samplePerPackage; }

  /*
   * 每个样本的字节配置。
    *默认为2个字节，目前仅支持2个字节。
   */
  void setBytesPerSample(vl_uint8 bytes = 2) { this->bytesPersample = bytes; }
  vl_uint8 getBytesPerSample() const { return this->bytesPersample; }

  /*
   * 指定此编码器的采样率，例如8000、16000。
    *并非编解码器将支持所有采样率，因此您必须检查
    *知道是否设置了采样率成功。
   */
  void setSampleRate(vl_uint32 sr) { this->sampleRate = sr; };
  vl_uint32 getSampleRate() const { return this->sampleRate; }
  /*
   *指定此编码器的比特率，例如8000、5000 ...
    *您必须检查编码器是否配置成功。
   */
  void setBitrate(vl_int32 br) { this->bitrate = br; };
  vl_int32 getBitrate() const { return this->bitrate; }

  /**
   * Get audio codec format id which based on codec type.
   */
  EAUDIO_FORMAT getFormatId() const { return formatId; }
protected:
  virtual vl_status updateParam() = 0;
  EAUDIO_FORMAT formatId;

  /* 选项值为VL_TRUE时，代表编码器支持，并且功能打开 */
  vl_bool enable_FEC;
  vl_bool enable_DTX;
  vl_bool enable_CBR;

  /* 此编解码器实例每个采样的当前字节，采样率和比特率设置 */
  vl_uint32 sampleRate;
  vl_int32 bitrate;
  vl_uint8 bytesPersample;
  vl_uint32 samplePerPackage;

  /* 循环缓冲区以在样本计数达到@samplePerPackage时缓存pcm */
  UnitCircleBuffer* circleBuffer;

  pthread_mutex_t stateMutex;
  vl_bool enabled;
};

/**
 * 解码器接口
 */
class AudioDecoder {
public:
  vl_bool isEnable() const { return enabled; }

  AudioDecoder(EAUDIO_FORMAT formatId, const AudioDecoderParam & decParam): enabled(VL_FALSE), formatId(formatId)
  {
    printf("AudioDecoder construct sr=%d, plc=%d",decParam.sampleRate, decParam.plc);
	setSampleRate(decParam.sampleRate);
	setPLC(decParam.plc);
	setBytesPerSample();
	pthread_mutex_init(&stateMutex, NULL);
  }

  virtual ~AudioDecoder() {
	pthread_mutex_destroy(&stateMutex);
  }

  //  vl_uint32 getFrameMS() const { return 20; };
  /**
   * 获取解码器解码一包数据所需的最大缓冲
   */
  //  virtual vl_size getPCMBufferSize() const = 0;
  /**
   * 解码方法
   */
  virtual vl_status decode(AudPacket* input, UnitCircleBuffer* circleBuffer) = 0;

  virtual vl_status getEncFramePCMInfo(PCMInfo * info) const {
    if(VL_TRUE != this->enabled) {
	  return VL_FAILED;
  	}
    /* 每20毫秒长宽比以获取新的pcm */
  	info->sample_cnt = getSampleRate() * 20  / 1000;
  	info->sample_rate = getSampleRate();
  	info->channel_cnt = 1;
  	info->bits_per_sample = getBytesPerSample() << 3;
	
    printf("AudioDecoder frame info sr=%d, sc=%d, bitpersample=%d", info->sample_rate, info->sample_cnt, info->bits_per_sample);
	
  	return VL_SUCCESS;
  }

  /*
   * 每个样本的字节配置。
    *默认为2个字节，目前仅支持2个字节。
   */
  void setBytesPerSample(vl_uint8 bytes = 2) { this->bytesPersample = bytes; }
  vl_uint8 getBytesPerSample() const { return this->bytesPersample; }


  /* 重新配置采样率 */
  virtual void setSampleRate(vl_uint32 sr) { this->sampleRate = sr; } 
  vl_uint32 getSampleRate() const { return this->sampleRate; }

  /* recofnig plc */
  vl_bool hasPLC() const { return enable_plc; }
  virtual vl_bool setPLC(vl_bool enable) { 
	enable_plc = VL_FALSE; 
	return enable_plc;
  }
  
  EAUDIO_FORMAT getFormatId() const { return formatId; }
protected:
  virtual vl_status updateParam() = 0;
  EAUDIO_FORMAT formatId;
  vl_uint32 sampleRate;
  vl_bool enable_plc;
  vl_uint8 bytesPersample;

  pthread_mutex_t stateMutex;
  vl_bool enabled;
};


#endif

