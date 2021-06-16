
#ifndef __VL_SESSION_HPP__
#define __VL_SESSION_HPP__

#include "vl_types.h"
#include "vl_const.h"
#include "vl_audio_codec.hpp"

/**
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
} VL_MED_EVENT;


/**
 * 媒体配置
 */
typedef struct {
  
} vl_aud_setting;

/**
 * 给上层的回调实例
 */
typedef struct {
  
} vl_med_callback;

/**
 * session回话，对应上层的一个群组结构
 * session由上层java初始化，主要配置群组对应的ip端口
 */
class vl_session
{
public:
  /**
   * 上层需提供目标网络地址, 媒体配置，私有目录，事件回调
   */
  vl_session(vl_char * ip, vl_uint16 port, const vl_aud_setting& aud_setting, vl_char * root_dir, vl_med_callback * cbptr = nullptr);
  vl_session(const vl_sockaddr_in& address, const vl_aud_setting& aud_setting, vl_char * root_dir, vl_med_callback * cbptr = nullptr);
  virtual ~vl_session();
  /**
   * 设置回调监听
   */
  void set_callback(vl_med_callback * cbptr);
  /**
   * 请求说话准备
   * 由于手机适配或录音设备被其他应用程序占用等原因，媒体模块准备可能失败
   * 结果由异步事件通知，失败则会给出错误原因
   */
  void speak_standby();
  /**
   * 准备接收语音
   * 上层由信令收到群众有说话请求，listen_standby将用于告知服务器本客户端的外网ip端口，需要服务器回复
   * 结果由异步事件通知，失败将给出错误原因
   */
  void listen_standby(const void *);
  /**
   * 通过该函数，告知服务器，某用户所在客户端的媒体ip/port
   */

private:
  /* 进入该session的本地时间戳 */
  vl_timestamp ts_in;
  /* 该session是否静音 */
  vl_bool silent;  // 
  /* session 对应的网络地址 */
  vl_sockaddr_in raddr;
  /* 媒体相关设置 */
  vl_aud_setting aud_setting;
  /* session 工作目录 */
  vl_char root_dir[VL_MAX_PATH];
  /* 事件回调实例，用于通知上层 */
  vl_med_callback * callback;
  /* 媒体编解码实例 */
  vl_audio_codec * codec;
  /* 表示该session外网ip端口上报有效期 */
  vl_uint32 alive_sec;
};

#endif

















