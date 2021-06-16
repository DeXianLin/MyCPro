#ifndef __CODEC_MANAGER_HPP__
#define __CODEC_MANAGER_HPP__

#include <stdlib.h>
#include <pthread.h>
#include <list>

#include "vl_types.h"
#include "AudioInfo.hpp"
#include "AudioCodec.hpp"
#include "Silk.hpp"

using namespace std;

/**
 * 编解码管理器(目前只支持silk)
 * 编解码管理器，可预先注册多种不同的编解码，包括编解码参数。编码管理器采用单例模式，使用时参数化获取编解码实例，每个具体的编解码应内部保证可重入性。
 * 获取默认编码器实例，又编码器优先级决定。
 * 编码和解码可配置的参数：
 * 编码：编码名字，PCM参数（采样率，声道数，sample大小，帧长度），压缩率（暂时分10个等级）,bps
 * 解码：解码只关心采样率
 *
 * 使用者可通过payload type获取编码器或解码器实例，预置的编解码器配置不可更改。预置的编解码器在管理器实例生成时初始化。
 * 使用者也可以传入特定的参数，从编解码器获得实例，该编解码器配置可由使用者在创建之后动态修改以适应不同的网络情况。
 */

#define MAX_BIT_RATE_COUNT                  (10)

#define CODECMGR_DEFAULT_ENC_QULITY    (8)
#define CODECMGR_DEFAULT_BANDTYPE      (WIDE_BAND)
#define CODECMGR_DEFAULT_FORMAT        (AUDIO_FORMAT_SILK)

typedef struct BuildinCodec {
  ERTP_PT pt;
  ECODEC_BAND_TYPE bt;
  //  AudioCodec * codec;
} BuildinCodec;

struct AudioEncoderInfo {
  ERTP_PT pt;
  const char* codecName;     /* codec name such as "silk", "evrc" */
  ECODEC_BAND_TYPE sampleRate;        /* sample rate that payload type indicated */
  int32_t bitrates[MAX_BIT_RATE_COUNT];         /* bit rate list that sample rate support  */
  vl_uint8 brArraySize;
};


class CodecManager {
public:
  static void setDefaultEncoder(EAUDIO_FORMAT format);
  static EAUDIO_FORMAT getDefaultEncoder();

  static void setDefaultEncoderComplex(int format);
  static int getDefaultEncoderComplex();

  /**
   * create audio encoder by default setting or use configuration param.
   */
  static AudioEncoder* createAudioEncoder(EAUDIO_FORMAT format = AUDIO_FORMAT_UNKNOWN);

  /**
   * create audio encoder by payload type, with specified bitrate and packet samples count.
   * codec type and sample rate deduce from payload type.
   * bitrate will set by the specific codec instance by default value, or specify by yourself
   * packageSampleCount caculate with 200ms base on sample rate, or specify by yourself
   */
  //  static AudioEncoder* createAudioEncoder(ERTP_PT payloadType, vl_uint32 packageSampleCount = 0, vl_int32 bitrate = 0);

  /**
   * create audio encoder by codec type and concrete parameter.
   */
  static AudioEncoder* createAudioEncoder(EAUDIO_FORMAT format, const AudioEncoderParam& param);


  /* create audio decoder by specified payload type */
  static AudioDecoder* createAudioDecoder(ERTP_PT payloadType);

  /* create audio decoder by codec type and concrete parameter */
  static AudioDecoder* createAudioDecoder(EAUDIO_FORMAT format, const AudioDecoderParam& param);


private:
  static vl_uint8 defaultPtIdx;
  static vl_uint8 defaultBrIdx;
  static EAUDIO_FORMAT defFormat;
  //  list<AudioCodec*> buildinCodes;
};

#endif

