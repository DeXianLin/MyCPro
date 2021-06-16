#ifndef __SILK_HPP__
#define __SILK_HPP__

#include <stdlib.h>
#include <pthread.h>
#include "vl_types.h"
#include "vl_const.h"
#include "AudioCodec.hpp"


#define SILK_DEFAULT_COMPLEXITY         (2)
#define SILK_ENC_CTL_PACKET_LOSS_PCT    (10)
#define SILK_MAX_BYTES_PER_FRAME        (1024)
#define SILK_MAX_API_FS_KHZ             (48)   // 最大支持的采样率
#define SILK_FRM_LENGTH_MS              (20)   // 一帧数据时间长度
#define SILK_MAX_FRAMES_PER_PACKET  5

#define __MALLOC malloc
#define __FREE free

class SilkEncoder :public AudioEncoder
{
  friend CodecManager;
public:
  static const char* name;

  SilkEncoder(const AudioEncoderParam& encParam);
  ~SilkEncoder();

  /**
   * Get max size to store a AudPacket. assume bitrate of 40kpbs (5bytes / ms)
   */
  vl_size getMaxPayloadSize() const;

  /**
   * Get max payload size per frame for silk
   */
  vl_size getMaxPayloadSizePerFrame() const;
  
  /**
   * Get encode audio packet if packet is ready.
   */
  vl_status getEncodedPacket(AudPacket* output);

  /**
   * get max duration per frame for silk encoder 
   */
  vl_uint32 getFrameMS() const { return SILK_FRM_LENGTH_MS; }
  
  /**
   * get max frame per packet for silk encoder.
   */
  vl_uint32 getMaxFramesPerPacket() const { return SILK_MAX_FRAMES_PER_PACKET; }

  /* silk支持属性 */
  vl_bool setDTX(vl_bool enable);
  vl_bool setFEC(vl_bool enable);

  vl_status getEncFramePCMInfo(PCMInfo * info) const;
protected:
  vl_status updateParam();
private:
  /* silk编码私有数据 */
  //SKP_SILK_SDK_EncControlStruct encControl;
  void *encState;
  static vl_uint8 bpSample[1];
};

class SilkDecoder : public AudioDecoder {
public:
  SilkDecoder(const AudioDecoderParam& param);
  ~SilkDecoder();

  vl_status getEncFramePCMInfo(PCMInfo * info) const;

  void enumBytesPerSample(vl_uint8** array, vl_size* size);

  vl_uint32 getFrameMS() const { return SILK_FRM_LENGTH_MS; }
  vl_size getPCMBufferSize() const { return getSampleRate() * getFrameMS() / 1000; }
  vl_status decode(AudPacket* input, UnitCircleBuffer* circleBuffer);
  vl_bool setPLC(vl_bool enable);
  vl_uint32 convertSampleRate(ECODEC_BAND_TYPE bandType) const;
protected:
  vl_status updateParam();
private:
  vl_status decode(vl_int8* packet, vl_size size, UnitCircleBuffer* circleBuffer);
  //SKP_SILK_SDK_DecControlStruct decControl;
  void* decState;
};

#endif
