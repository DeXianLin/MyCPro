/**
   android 声音设备操作类，使用opensles实现
 */


#ifndef __VL_AUD_DEV_HPP__
#define __VL_AUD_DEV_HPP__

#include "vl_types.h"
#include "vl_circle_buf.hpp"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>
#include <sys/system_properties.h>
#include <android/api-level.h>

#define NUM_BUFFERS 2

typedef struct 
{
  vl_uint32 sample_rate; // 采样率，如 8000, 16000 ...
  vl_uint32 channel_cnt; // 通道个数， 如 1， 2
  vl_uint32 streamType;
  vl_uint32 bits_per_sample;
  vl_uint32 frm_duration;
} vl_aud_dev_rec_param;

typedef struct
{
  vl_uint32 sample_rate;
  vl_uint32 channel_cnt;
  vl_uint32 streamType;
  vl_uint32 bits_per_sample;
  vl_uint32 frm_duration; // 影响缓存区大小，单位毫秒
  vl_uint32 volume;
} vl_aud_dev_ply_param;

/**
 * 用于resample，不同客户端之间的通讯，可能由于不同的带宽或其他原因，采样率，频道数，单位采样
 * 不统一，因此需要
 */
typedef struct
{
  
} vl_aud_dev_param;


class vl_aud_dev
{
private:
  /* 播放 + 录音公用引擎，private数据类型，不是用types */
  SLObjectItf         engineObject;
  SLEngineItf         engineEngine;
  SLObjectItf         outputMixObject;

  /* 播放 */
  SLObjectItf         playerObj;
  SLPlayItf           playerPlay;
  SLVolumeItf         playerVol;
  SLmillibel          originVol;
    
  /* 录音 */
  SLObjectItf         recordObj;
  SLRecordItf         recordRecord;

  /* 缓冲队列 */
  SLAndroidSimpleBufferQueueItf playerBufQ;
  SLAndroidSimpleBufferQueueItf recordBufQ;
  
  void initial();
  vl_status create_recorder(vl_aud_dev_rec_param * rec_param);
  vl_status create_player(vl_aud_dev_ply_param * ply_param);
  void destroy_recorder();
  void destroy_player();

public:
  /* 缓冲区变量，由于传入回调函数，public */
  vl_bool             playing;
  vl_circle_buf      *inputBuff;
  unsigned            playerBufferSize;
  char               *playerBuffer[NUM_BUFFERS];
  int                 playerBufIdx;

  vl_bool             recording;
  vl_circle_buf      *outputBuff;
  unsigned            recordBufferSize;
  char               *recordBuffer[NUM_BUFFERS];
  int                 recordBufIdx;

  vl_aud_dev(vl_aud_dev_param * param);
  ~vl_aud_dev();

  /* 静态初始化声音设备 */
  vl_status open(vl_aud_dev_rec_param * rec_param, vl_aud_dev_ply_param * ply_param);
  /* 关闭声音设备 */
  vl_status close();
  /* 动态修改录音参数 */
  vl_status reconf_rec(vl_aud_dev_rec_param * param);
  /* 动态修改声音播放参数 */
  vl_status reconf_ply(vl_aud_dev_ply_param * param);
  /* 设置i/o虚拟端口 */
  void set_play_input(vl_circle_buf * play_input);
  void set_record_output(vl_circle_buf * record_output);

  /* 录音 */
  vl_status record_start();
  vl_status record_pause();
  vl_status record_resume();
  vl_status record_stop();

  /* 播放 */
  vl_status play_start();
  vl_status play_pause();
  vl_status play_resume();
  vl_status play_mute(vl_bool is_mute);
  vl_status play_set_volume(vl_int16 volume);
  vl_status play_stop();
};

#endif











