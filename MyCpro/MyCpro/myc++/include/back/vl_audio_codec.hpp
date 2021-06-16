
#ifndef __VL_AUDIO_CODEC_HPP__
#define __VL_AUDIO_CODEC_HPP__

#include "vl_types.h"
#include "vl_aud_frm_buf.hpp"

#define MAX_VL_CODEC_NAME (20)
#define VL_MAX_CODEC_QULITY  (10)
#define VL_MIN_CODEC_QULITY  (1)

typedef struct vl_audio_codec_params
{
  vl_uint32 sample_rate;  // 采样率，与PCM采样率一致
  vl_uint32 channel_cnt;  // 声道数，与PCM一致
  //  vl_uint32 avg_bps; // 平均比特率，部分编解码使用
  vl_uint32 max_bps; // 最大比特率，用于定义缓冲区大小
  vl_uint16 dec_ftime; // 一帧pcm时长
  vl_uint16 enc_ftime; // 一帧pcm时长
  vl_uint8 pcm_bits_per_sample;  // 一个sample占用bit位数

  vl_uint8 enc_qulity;      // 编码音质   1 - 10
  
  vl_bool use_dtx;  // 是否使用不连续传输
  vl_bool use_fec;  // 是否启用向前纠错功能（使能会增大网络传输量）
  
  vl_uint8 frm_per_pkt; // 一个传输包包含的帧数（1~5）
  vl_bool vad; 
  vl_bool cng;
  vl_bool penh;
  vl_bool plc;

} vl_audio_codec_params;

class vl_audio_codec
{
protected:
  vl_char codec_name[MAX_VL_CODEC_NAME];
  void * codec_private; // 不同编解码器似有数据成员
public:
  vl_bool enc_enable;
  vl_bool dec_enable;

  vl_audio_codec();
  virtual ~vl_audio_codec();

  virtual vl_status open(vl_audio_codec_params * param) = 0;  // 编解码初始化
  virtual vl_status close() = 0; // 关闭编解码
  virtual vl_status modify(vl_audio_codec_params * param) = 0; // 重新设置编解码参数，用于流控
  /* 将编码包解析为帧，修改帧的bit_info, 时间戳 */
  virtual vl_status parse(void * pkt, vl_size pkt_size, vl_timestamp ts, vl_uint16 * frm_count, vl_audio_frame frames[]) = 0; 
  virtual vl_status encode(vl_audio_frame * input, vl_audio_frame * output) = 0; // 编码
  virtual vl_status decode(vl_audio_frame * input, vl_audio_frame * output) = 0; // 解码
  virtual vl_status recover(vl_size out_size, vl_audio_frame * output) = 0;   // 用于解码
  virtual vl_status adjust_qulity(vl_int8 adjust) = 0; // 调整编码质量
};

#endif












