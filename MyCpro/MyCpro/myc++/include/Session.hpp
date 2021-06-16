#ifndef __SESSION_HPP__
#define __SESSION_HPP__

#include <list>
#include <map>
#include <utility>
#include "vl_types.h"
#include "vl_const.h"
#include "Socket.hpp"
#include "EndPoint.hpp"
#include "EndPointManager.hpp"
#include "AudioCodec.hpp"
#include "OutgoingTransaction.hpp"
#include "IncomingTransaction.hpp"


using namespace std;

#define SESSION_DEFAULT_LPORT          (0)
#define SESSION_DEFAULT_OUT_QULITY     (8)
#define SESSION_DEFAULT_OUT_BANDTYPE   (WIDE_BAND)

#define SESSION_INVALID_HANDLER        (0)

class Session;

/**
 * Session流程
 * 
 * 媒体jni层上报事件，可能会带参数
 */
typedef enum {
  /* 成功结束某次语音(发送/接收) */
  EV_TRANS_DONE = 0,
  /******************** 发送环节 ********************/
  /* 录音设备发生未知错误，例如采样率不支持 */
  EV_AUD_RECORD_ERR,
  /* 录音设备被占用 */
  EV_AUD_RECORD_OCCUPY,
  EV_AUD_RECORD_START,
  EV_AUD_RECORD_STOP,
  /* 网卡设备没有准备好，指定时间内媒体数据无法发送 */
  EV_RTP_OUTGOING_TIMEOUT,

  /******************** 接收环节 ********************/
  /* 声音播放设备发生未知错误 */
  EV_AUD_PLAY_ERR,
  /* 声音播放通道被占用 */
  EV_AUD_PLAY_OCCUPY,
  EV_AUD_PLAY_START,
  EV_AUD_PLAY_STOP,
  /* 客户端已将本地的外网ip端口同步给服务器，可以开始接收rtp */
  EV_RTP_RECV_READY,
  /* session指定的端口指定时间内没有收到指定ssrc信源的rtp数据 */
  EV_RTP_INCOMING_TIMEOUT,
  /* 已接受到指定ssrc信源的数据，但在抖动缓冲内没有收到第一帧 */
  EV_RTP_LACKOF_START_FRAME,
  /* 信令已通知语音结束，指定ssrc信源的rtp流已经中断，并且指定时间内仍然没有受到最后一帧 */
  EV_RTP_LACKOF_END_FRAME,
  /* 从接收的rtp序号看，rtp丢包严重，超过某个罚值*/
  EV_RTP_LOSS_SEVERE,
  /* 未收到信令结束，指定时间内没有受到后续rtp包，而且没有收到rtp结束包 */
  EV_RTP_BREAK_OFF,
  /* 未收到信令结束，已经收到rtp结束包， 并且指定时间内没有收到该信源序列号更大的rtp */
  EV_RTP_CLOSURE,

  /* 录音失败，磁盘空间满 */
  EV_RECORD_DISK_FULL,
  /* 录音失败，未提供目录 */
  EV_RECORD_NO_DIR,
  /* 录音失败 */

  /* 操作成功 */
  MSG_SUCCESS,
  /* 异步操作，等待回调 */
  MSG_WAIT,
  /* 下层错误 */
  MSG_ERROR,
  /* transaction关闭成功 */
  MSG_TRANS_CLOSE_DONE,
  /* transaction关闭超时 */
  MSG_TRANS_CLOSE_TIMEOUT,
  /* transaction接收超时 */
  MSG_TRANS_RECV_TIMEOUT,
  /* transaction 不存在 */
  MSG_TRANS_NOT_EXIST,
  /* 启动声音设备失败 */
  MSG_AUDDEV_ERROR,
  /* 声音设备被占用 */
  MSG_AUDDEV_OCCUPIED,
  /* 编解码不支持 */
  MSG_AUD_CODEC_UNSUP,
  /* 参数配置有误 */
  MSG_INVALID_PARAM,
} SessionMsg;

typedef struct {
  vl_uint32 ssrc;
} SessionCBParam;

class SessionCallback {
public:
  virtual void* onEvent(SessionMsg event, const SessionCBParam& param) = 0;
};

/*
 * Session对应上层的群概念，一个群对应一个外网的ip：port
 * 向上提供发送/接收语音接口。主动发送或接收时需要传入单次读音id(ssrc)。
 * Session本身并不限制同时接收语音的数量，上层自由定义播放策略，即Session可同时管理多个活动的Transaction。
 * 考虑信令和rtp语音时延问题，Session提供自动接收模式，在上层没有主动调用接收语音时，
 * Session可自动接收语音数据并缓冲。若上层在一定语音帧数量后没有主动调用接收，自动关闭队列的读端。
 * 
 * Session可打开本地文件存储，在同时收到几个语音时，可以自动保存到文件，文件将根据ssrc自动命名，存放到root_dir中。
 *
 *
 * 注意：ssrc 32bit，单个群里文件名冲突概率不高，后来的文件将覆盖前面的文件。
 * dangling 不处理语音流
 */
class Session {

public:
  Session(vl_uint32 rip, vl_uint16 rport, const char* root_dir = NULL, vl_uint32 engrossId = SPS_NORMAL_PERMIT, vl_uint16 lport = SESSION_DEFAULT_LPORT);
  Session(const char* rip, vl_uint16 rport, const char* root_dir = NULL, vl_uint32 engrossId = SPS_NORMAL_PERMIT, vl_uint16 lport = SESSION_DEFAULT_LPORT);
  virtual ~Session();

  /* 设置回调 */
  void setCallback(SessionCallback* callback) { this->callback = callback; }

  /* 打开本地存储 */
  void enableRecordFile(bool enable) { this->recordFile = enable; }
  vl_bool isRecordFile() const { return recordFile;}

  

  /* 是否静音 */
  void setMute(vl_bool mute) { this->mute = mute; }
  vl_bool isMute() const { return this->mute; }

  /* 设置心跳，可以接收流媒体 断开心跳 */
  void standby(EptHeartBeat& heartBeat);
  void disconnect();

  /**
   * 发送语音相关接口
   * 创建时，返回ssrc。通过rtpExtension传递附加数据，extId暂时可以忽略
   */
  vl_uint32 createOutgoingTransaction(TransactionOutputParam& param);
  vl_uint32 createOutgoingTransaction(vl_uint32 ssrc, void* rtpExtension, vl_size extensionLen, vl_uint16 extId);
  SessionMsg startSpeak(vl_uint32 handler);
  SessionMsg stopSpeak(vl_uint32 handler);
  SessionMsg releaseOutgoingTransaction(vl_uint32 handler, vl_uint32 waitMsec = 2000);
  
  /**
   * 接收语音相关接口
   * 创建时，返回ssrc。通过rtpExtension传递附加数据，extId暂时可以忽略
   */
  vl_uint32 createIncomingTransaction(vl_uint32 ssrc, vl_bool isLocal = VL_FALSE);
  SessionMsg startPlayVoice(vl_uint32 handler);
  SessionMsg stopPlayVoice(vl_uint32 handler);
  SessionMsg releaseIncomingTransaction(vl_uint32 handler, vl_uint32 waitMsec = 2000);

  void cleanOutgoingTransaction();

private:
  /* 通知上层事件 */
  void invokeEvent(SessionMsg event, const SessionCBParam& param);
  /* 默认初始化 */
  vl_bool defaultInitial(const char* rootDir, vl_uint16 lport);
  /* 全局发送和接受包的内存缓冲池 */
  static SimpleMemoryPool simpleMemPool;
  /* 心跳地址，ioq过滤 */
  SockAddr sockAddr;
  /* 是否打开本地存储，默认打开 */
  vl_bool recordFile;
  /* 进入该session的本地时间戳 */
  vl_timestamp ts_in;
  /* 群回调 */
  SessionCallback* callback;
  /* 群根目录 */
  vl_uint8 root_dir[VL_MAX_PATH];
  /* 是否静音，默认关闭 */
  vl_bool mute;
  /* 是否播放状态 */
  vl_bool playing;
  /* 是否正在录音 */
  vl_bool recording;
  /* 群是否准备完成 */
  vl_bool ready;
  /* 本地端点 */
  shared_ptr<EndPoint> pEpt;

  vl_uint32 engrossId;

  /* 当前活动的语音 */
  map<vl_uint32, OutgoingTransaction*> outgoingMap;
  pthread_mutex_t outgoingMapLock;
  vl_bool pushOutgoingTransaction(OutgoingTransaction* outgoing);
  void popOutgoingTransaction(vl_uint32 ssrc);
  OutgoingTransaction* findOutgoingTransaction(vl_uint32 ssrc);

  map<vl_uint32, IncomingTransaction*> incomingMap;
  pthread_mutex_t incomingMapLock;
  vl_bool pushIncomingTransaction(IncomingTransaction* incoming);
  void popIncomingTransaction(vl_uint32 ssrc);
  IncomingTransaction* findIncomingTransaction(vl_uint32 ssrc);
};

#endif
