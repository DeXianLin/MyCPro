#ifndef __VL_IOQ_HPP__
#define __VL_IOQ_HPP__

#include <queue>
#include "vl_types.h"
#include "vl_aud_frm_buf.hpp"

using namespace std;

struct vl_aud_frm_cmp
{  
  bool operator()(const vl_audio_frame &na, const vl_audio_frame &nb) {  
	return na.timestamp < nb.timestamp;
  }  
};  

class vl_aud_frm_ioq
{
private:
  priority_queue<vl_audio_frame, vector<vl_audio_frame>, vl_aud_frm_cmp> sendq;
  priority_queue<vl_audio_frame, vector<vl_audio_frame>, vl_aud_frm_cmp> recvq;
public:
  vl_aud_frm_ioq();
  ~vl_aud_frm_ioq();
  
  void send_enqueue(const vl_audio_frame&);
  const vl_audio_frame& send_dequeue();

  void recv_enqueue(const vl_audio_frame&);
  const vl_audio_frame& recv_dequeue();
};

#endif

















