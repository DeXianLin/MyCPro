#ifndef __SILK_FILE_STREAM_SOURCE__
#define __SILK_FILE_STREAM_SOURCE__


#include "vl_types.h"
#include "vl_const.h"
#include "SilkFile.hpp"
#include "StreamPlaySource.hpp"
#include "AudioDevMgr.hpp"
#include "AudioCodec.hpp"

class SilkFileStreamSource : public StreamPlaySource, public PCMFeeder
{
public:
  /* 准备就绪 */
  virtual vl_status spsStandby();
  /* 开始播放 */
  virtual vl_status spsStartTrack();
  /* 暂停播放 */
  virtual vl_status spsPauseTrack();
  /* 恢复播放 */
  virtual vl_status spsResumeTrack();
  /* 结束播放 */
  virtual vl_status spsStopTrack();
  /* 释放相关资源 */
  virtual vl_status spsDispose();
  /* 判断是否可以播放 */
  virtual vl_bool spsReadyForPlay() const;
  /* 判断是否可以释放资源 */
  virtual vl_bool spsReadyForDestroy();
  /* 获取唯一标志 */
  virtual vl_uint32 spsGetIdentify() const; 
  /* 获取播放状态 */
  virtual SPS_STATE spsGetPlayState() const;
  virtual void spsSetPlayState(SPS_STATE newState);
  /* 播放结束，获取播放结果 */
  virtual void spsGetResultInfo(struct SpsResultInfo& info) const;
  virtual void spsMarkPlayed();

  /* 用于提pcm数据接口 */
  virtual vl_status feedPcm(PCMInfo* pcmInfo, UnitCircleBuffer* circleBuffer);

  vl_status stopTrackInternal(vl_bool isPause = VL_FALSE);

  ~SilkFileStreamSource();
  SilkFileStreamSource(const char* filePath, vl_uint32 identity);

  void notifyScheduler();

  
private:
  UnitCircleBuffer* circleBuffer;
  SilkStorage silkFile;
  AudioDecoder* decoder;
  vl_uint32 identity;
  SPS_STATE playState;
  int audioHandler;
};


#endif

