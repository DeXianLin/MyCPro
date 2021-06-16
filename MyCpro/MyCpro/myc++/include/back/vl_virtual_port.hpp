/**
   虚拟传输通道接口，用于传输和缓冲声音帧数据，有输入通道和输出通道，
*/


#ifndef __VL_VIRUTAL_PORT_HPP__
#define __VL_VIRUTAL_PORT_HPP__

#include "vl_types.h"
#include "vl_aud_frm_buf.hpp"

class vl_virtual_port
{
  vl_virtual_port();
  virtual ~vl_virtual_port() = 0;
  virtual vl_status get_frame(vl_audio_frame * frm) = 0;
  virtual vl_status put_frame(vl_audio_frame * frm) = 0;
};

#endif
















