

#ifndef __VL_ORDER_LIST_HPP__
#define __VL_ORDER_LIST_HPP__

#include "vl_aud_frm_buf.hpp"

class vl_audfrm_ordeded_queue
{
private :
  vl_audio_frame * ptr_head;
  vl_audio_frame * ptr_tail;
  vl_size len;   /* item长度 */
  vl_size cap;   /* 容量 */ 
public :
  vl_status enqueue(vl_audio_frame * item);
  vl_status dequeue(vl_audio_frame ** item);
};

#endif















