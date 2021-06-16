#ifndef __STREAM_PLAY_SOURCE__
#define __STREAM_PLAY_SOURCE__

#include "vl_types.h"
#include "vl_const.h"

#define SPS_NORMAL_PERMIT (0)

enum IncomingErrno{
  EIN_NORMAL = 0,
  EIN_UNKNOWN_MEDIA,
  EIN_TIMEOUT
};

#pragma pack(1)
struct RecvTransInfo{
  /* 用于异常分析 */
  uint64_t first_recv_ts;    // 收到第一个包的本地时间戳
  uint64_t play_ts;          // 播放时候本地时间戳
  int32_t max_jitter;        // 最大抖动
  int32_t total_jitter;      // 总抖动
  uint32_t max_seq;          // 最大序号，代表理论上发送放发送的总包数

  /* 会话信息 */
  uint32_t ssrc;             // ssrc
  uint32_t send_id;          // 发送放uin
  uint32_t group_id;         // 若为群聊，代表群号，否则为0
  uint32_t recv_id;          // 接收端的uin
  uint16_t flag;             // 0 点对点， 1 群聊

  /* 重点关注参数 */
  uint16_t dur_per_pack;     // 每个udp包带有流媒体声音长度
  uint16_t block_times;      // 断续次数
  uint8_t normalEnd;         // 正常结束（有受到结束帧）
  uint8_t lost_persent;     // 丢包率百分比
  uint8_t send_os;           // 发送端平台 : 0 表示android， 1 表示ios
  uint8_t recv_os;           // 接收端平台 : 0 表示android， 1 表示ios
  uint8_t send_net_type;     // 发送端网络信息：1表示WIFI，2表示2G，3表示3G，0为了兼容前面版本，表示未上报类型或未知类型
  uint8_t recv_net_type;     // 接收端网络信息：1表示WIFI，2表示2G，3表示3G，0为了兼容前面版本，表示未上报类型或未知类型
};
#pragma pack()

struct FileStreamInfo
{
    int a;
};

union SpsStreamInfoDetail {
  struct RecvTransInfo recfInfo;
  struct FileStreamInfo fileInfo;
};

enum SpsResultType {
  SPS_TYPE_UDP,
  SPS_TYPE_FILE
};

struct SpsResultInfo {
  IncomingErrno lastError;
  vl_uint16 lostCount;
  vl_uint16 mediaCount;
  vl_uint8 extType;  // 用于标记附加，0表示网络流，1表示文件流。网络流具体信息记录为RecvTransInfo
  SpsStreamInfoDetail streamInfoDetail;
};

enum SPS_STATE {
  SPS_INVALID,
  SPS_WAITING,
  SPS_PLAYING,
  SPS_PAUSED,
  SPS_NO_MORE,
  SPS_PLAYED
};

enum SRS_STATE {
  SRS_INVALID,     // 创建，不可用状态
  SRS_INITIALED,   // 已初始化
  SRS_WAITING,     // 等待录音
  SRS_RECORING,    // 正在录音
  SRS_STOPING,     // 停止录音中
  SRS_STOPED       // 录音结束
};

/**
 * 流媒体播放源，现支持实时流，文件流
 */
class StreamPlaySource
{
public:
  /* 准备并等待播放调度 */
  virtual vl_status spsStandby() = 0;
  /* 开始播放 */
  virtual vl_status spsStartTrack() = 0;
  /* 暂停播放 */
  virtual vl_status spsPauseTrack() = 0;
  /* 恢复播放 */
  virtual vl_status spsResumeTrack() = 0;
  /* 结束播放 */
  virtual vl_status spsStopTrack() = 0;
  /* 释放相关资源 */
  virtual vl_status spsDispose() = 0;
  /* 判断是否可以播放 */
  virtual vl_bool spsReadyForPlay() const = 0;
  /* 判断是否可以释放资源 */
  virtual vl_bool spsReadyForDestroy() = 0;
  /* 获取唯一标志 */
  virtual vl_uint32 spsGetIdentify() const = 0;
  /* 获取播放状态 */
  virtual SPS_STATE spsGetPlayState() const = 0;
  virtual void spsSetPlayState(SPS_STATE newState) = 0;
  /* 播放结束，获取播放结果 */
  virtual void spsGetResultInfo(struct SpsResultInfo& info) const = 0;
  /* 标记该流不会被播放 */
  virtual void spsMarkPlayed() = 0;

  /* 普通权限 */
  StreamPlaySource(vl_uint32 permitId = SPS_NORMAL_PERMIT) : permitId(permitId) {}
  /* 获取许可ID */
  vl_uint32 spsGetPermitId() const { return permitId; }
  /* 析构函数 */
  virtual ~StreamPlaySource() {}
private:
  vl_uint32 permitId;
};

class StreamRecordSource {
public:
  virtual ~StreamRecordSource(){}
  // 录音初始化
  virtual vl_status srsStandby() = 0;
  // 询问是否准备好可以开始录音
  virtual bool srsReadyForRecord() = 0;
  // 开始录音
  virtual vl_status srsStartRecord() = 0;
  // 结束录音, async 为true，表示等待缓冲区数据全部读出才结束
  virtual vl_status srsStopRecord(bool async) = 0;
  // 询问是否可以delete
  virtual bool srsReadyForDestroy() = 0;
  /* 询问是否释放外围资源 */
  virtual bool srsReadyForDispose() = 0;
  // 释放外围资源
  virtual void srsDispose() = 0;
  // 获取录音状态
  virtual SRS_STATE srsGetRecrodState() = 0;
};

#endif


















