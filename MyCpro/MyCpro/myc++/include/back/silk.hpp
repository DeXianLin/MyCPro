
#ifndef __VL_AUDIO_CODEC_SILK_H__
#define __VL_AUDIO_CODEC_SILK_H__


#include "vl_types.h"

#include "SKP_Silk_SDK_API.h"
#include "vl_audio_codec.hpp"


typedef struct silk_private
{
  vl_uint32 sample_per_frame;
  vl_uint32 pcm_bytes_per_sample;

  vl_uint8 enc_qulity;
  SKP_SILK_SDK_EncControlStruct enc_ctl;
  void *enc_st;

  SKP_SILK_SDK_DecControlStruct dec_ctl;
  void * dec_st;

  void        *dec_buf[SILK_MAX_FRAMES_PER_PACKET-1];
  SKP_int16    dec_buf_size[SILK_MAX_FRAMES_PER_PACKET-1];
  vl_size      dec_buf_sz; //默认缓冲区大小
  vl_uint32    dec_buf_cnt;
  vl_uint32    pkt_info;    /**< Packet info for buffered frames.  */  
} silk_private;


class vl_audio_codec_silk : public vl_audio_codec
{
public:
  vl_audio_codec_silk();
  ~vl_audio_codec_silk();

  vl_status open(vl_audio_codec_params * param);
  vl_status close();
  vl_status modify(vl_audio_codec_params * param);
  vl_status parse(void * pkt, vl_size pkt_size, vl_timestamp ts, vl_uint16 * frm_count, vl_audio_frame frames[]);
  vl_status encode(vl_audio_frame * input, vl_audio_frame * output);
  vl_status decode(vl_audio_frame * input, vl_audio_frame * output);
  vl_status recover(vl_size out_size, vl_audio_frame * output);
  vl_status adjust_qulity(vl_int8 adjust);
};


#endif







