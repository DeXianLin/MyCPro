
#ifndef __VL_AUD_MANAGER_HPP__
#define __VL_AUD_MANAGER_HPP__

#include "vl_types.h"
#include <pthread.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>


/**
 * 负责管理声音设备，单例模式
 * 不同群之间共享一个媒体管理实例，但每个群可以有部分不同的语音配置。需要统一的参数如下：
 * 同一时间，只能有一个声音播放，也只能有一个录音。
 * 
 * 关于声音设备，不同群主要靠不同的语音Filter做处理
 */

#define NUM_BUFFERS (2)
#define MAX_VOLUME  (32)

typedef struct {
  vl_int16 init_vol;
} vl_aud_dev_param;

/**
 * 录音参数(每次启动录音时传入)
 */
typedef struct vl_aud_rec_param {
  /* 采样率，如 8000, 16000 ... */
  vl_uint32 sample_rate;
  /* 声道数 */
  vl_uint32 channel_cnt;
  /* 每个sample占用比特数 */
  vl_uint32 bits_per_sample;
  /* 一帧数据的时长 */
  vl_uint32 frm_duration;

  vl_aud_rec_param& operator=(const vl_aud_rec_param& param) {
	sample_rate = param.sample_rate;
	channel_cnt = param.channel_cnt;
	bits_per_sample = param.bits_per_sample;
	frm_duration = param.frm_duration;
	return * this;
  }

  bool operator==(const vl_aud_rec_param& param) {
	if(sample_rate == param.sample_rate &&
	   channel_cnt == param.channel_cnt &&
	   bits_per_sample == param.bits_per_sample &&
	   frm_duration == param.frm_duration) {
	  return true;
	} else {
	  return false;
	}
  }
} vl_aud_rec_param;

/**
 * 播放参数(每次启动播放流时传入)
 */
typedef struct vl_aud_ply_param {
  /* 采样率，如 8000, 16000 ... */
  vl_uint32 sample_rate;
  /* 声道数 */
  vl_uint32 channel_cnt;
  /* 每个sample占用比特数 */
  vl_uint32 bits_per_sample;
  /* 影响缓存区大小，单位毫秒 */
  vl_uint32 frm_duration;
  /* 标志从feeder中获取不到新的pcm时，是否自动释放播放设备，用于文件播放 */
  vl_bool   auto_release;
  /* 指定从那个声音通道播放, 设置为 <0 时候，默认使用music通道 */
  SLint32   streamType;

  vl_aud_ply_param& operator=(const vl_aud_ply_param& param) {
	sample_rate = param.sample_rate;
	channel_cnt = param.channel_cnt;
	bits_per_sample = param.bits_per_sample;
	frm_duration = param.frm_duration;
	auto_release = param.auto_release;
	streamType = param.streamType;
	return * this;
  }

  bool operator==(const vl_aud_ply_param& param) {
	if(sample_rate == param.sample_rate &&
	   channel_cnt == param.channel_cnt &&
	   bits_per_sample == param.bits_per_sample && 
	   frm_duration == param.frm_duration &&
	   auto_release == param.auto_release &&
	   streamType == param.streamType) {
	  return true;
	} else {
	  return false;
	}
  }

  /* 用于判断新的参数是否需要重启播放器 */
  vl_bool need_restart(const vl_aud_ply_param& new_param) {
	if((sample_rate != new_param.sample_rate) || 
	   (channel_cnt != new_param.channel_cnt) ||
	   (bits_per_sample != new_param.bits_per_sample) ||
	   (frm_duration != new_param.frm_duration)) {
	  return VL_TRUE;
	} else {
	  return VL_FALSE;
	}
  }
} vl_aud_ply_param;
/**
 * 提供pcm流接口，播放回调
 */
class vl_aud_raw_frm_feeder
{
  virtual vl_status get_pcm(vl_aud_ply_param * param, vl_uint8 * samples, vl_size * sample_count);
};
/**
 * 录音回调接口
 */
class vl_aud_raw_frm_consumer
{
  virtual vl_status put_pcm(vl_aud_rec_param * param, const vl_uint8 * samples, vl_size * sample_count);
};

/**
 * 声音设备管理类
 * 不允许同时录音
 * 不允许同时在同一个声道播放
 */


class vl_aud_manager {
public :
  /* 获取媒体设备管理实例 */
  static vl_aud_manager& get_instance(vl_aud_dev_param * init_param);
  /* 请求播放 */
  vl_status aquire_play(const vl_aud_ply_param& param, vl_aud_raw_frm_feeder * feeder, int * playerId);
  /* 加大音量 */
  vl_status inc_volume();
  /* 减少音量 */
  vl_status dec_volume();
  /* 设置音量, 音量值范围为32 */
  vl_status set_volume(vl_int16 volume);
  /* 暂停播放 */
  vl_status pause_play(int playerId);
  /* 恢复播放 */
  vl_status resume_play(int playerId);
  /*开始播放, 传入aquire的id */
  vl_status start_play(int playerId);
  /* 停止播放 */
  vl_status stop_play(int playerId, vl_bool wait);
  /* 释放播放设备 */
  vl_status release_play_dev(int playerId);
  /* 请求录音 */
  vl_status aquire_record(const vl_aud_rec_param& param, vl_aud_raw_frm_consumer * consumer, int * recordId);
  /* 开始录音 */
  vl_status start_record(int recordId);
  /* 停止录音 */
  vl_status stop_record(int recordId);
  /* 释放录音设备 */
  vl_status release_rec_dev(int recordId);
private :
  vl_aud_manager(vl_aud_dev_param * init_param);
  virtual ~vl_aud_manager();

  void reset();
  vl_status reinitial(vl_aud_dev_param * init_param);
  void deinitial();
  /* 等待缓冲区数据结束 */
  void play_wait();

  static vl_aud_manager * instance;
  /* opensl 可用 */
  vl_bool             isReady;

  /* 内部播放缓冲区 */
  vl_bool             playing;
  unsigned            playerBufferSize;
  char               *playerBuffer[NUM_BUFFERS];
  int                 playerBufIdx;
  vl_aud_raw_frm_feeder * feeder;
  vl_aud_ply_param    playerParam;
  int                 playerId;
  pthread_mutex_t     playerLock;

  /* 内部录音缓冲区 */
  vl_bool             recording;
  unsigned            recordBufferSize;
  char               *recordBuffer[NUM_BUFFERS];
  int                 recordBufIdx;
  vl_aud_raw_frm_consumer * consumer;
  vl_aud_rec_param    recordParam;
  int                 recordId;
  pthread_mutex_t     recordLock;

  /* 播放 + 录音公用引擎，private数据类型，不是用types */
  SLObjectItf         engineObject;
  SLEngineItf         engineEngine;
  SLObjectItf         outputMixObject;

  /* 播放 */
  SLObjectItf         playerObj;
  SLPlayItf           playerPlay;
  SLVolumeItf         playerVol;
  vl_int16            adjustVol;
  SLmillibel          originVol;

  /* 录音 */
  SLObjectItf         recordObj;
  SLRecordItf         recordRecord;

  /* 缓冲队列 */
  SLAndroidSimpleBufferQueueItf playerBufQ;
  SLAndroidSimpleBufferQueueItf recordBufQ;
};

vl_aud_manager* vl_aud_manager::instance = NULL;

#endif



