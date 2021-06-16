#ifndef __VL_TRANSACTION_HPP__
#define __VL_TRANSACTION_HPP__

#include "vl_types.h"
#include "vl_const.h"
#include "vl_ioq.hpp"

typedef enum {
  /* 接收语音流 */
  DIR_INCOMING,
  /* 发送语音流 */
  DIR_OUTGOING
} VL_TRANS_DIR;

typedef enum {
  
} VL_TRANS_OUT_STATE;

typedef enum {
  
} VL_TRANS_IN_STATE;

typedef union {
  VL_TRANS_OUT_STATE out_state;
  VL_TRANS_IN_STATE in_state;
} VL_TRANS_STATE;

/**
 * 代表客户端一次讲话或接收语音完整过程
 * 
 */
class vl_transaction
{
  
private:
  /* 语音流方向 */
  VL_TRANS_DIR dir;
  /* 本次语音转发状态 */
  VL_TRANS_STATE state;
  /* 语音流id，由上层信令交换并传到多媒体层 */
  vl_uint32 ssrc;
  /* 第一个语音流本地时间戳 */
  vl_timestamp rtp_fst_ts;
  /* 语音流保存文件路径，若为空，则不保存 */
  vl_char * save_path;
  /* 会议持有ioq指针 */
  vl_aud_frm_ioq * ioq;
};

#endif

















