#ifndef __VL_FRM_PIPE_HPP__
#define __VL_FRM_PIPE_HPP__

/* 
 * 存放已经编码的语音流的单向管道
 * 可注册为发送或接收的管道
 * 在该管道中可有过滤器，例如保存音频文件就放在这个功能中
 */

#include "vl_types.h"
#include "vl_aud_frm_buf.hpp"

#include <queue>

using namespace std;

struct vl_aud_frm_cmp
{  
  bool operator()(const vl_audio_frame &na, const vl_audio_frame &nb) {  
	return na.timestamp < nb.timestamp;
  }  
};  

class vl_frm_pipe 
{
  vl_frm_pipe();
  vl_audio_frame& read();
  vl_status write(vl_audio_frame& frm);
  vl_status close_read();
  vl_status open_read();
private:
  priority_queue<vl_audio_frame, vector<vl_audio_frame>, vl_aud_frm_cmp> buf;
  
};

#endif











