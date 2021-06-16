
#include "vl_types.h"

typedef enum
{
  VL_AUD_FRM_TYPE_PCM_8,
  VL_AUD_FRM_TYPE_PCM_16,
  VL_AUD_FRM_TYPE_PCM_24,
  VL_AUD_FRM_TYPE_PCM_32,
  VL_AUD_FRM_TYPE_SILK
} vl_frame_type;

typedef struct vl_audio_frame
{
  void * buf;
  vl_size size;  // 有效数据大小
  vl_size capacity;  // buff内存容量
  vl_timestamp timestamp;
  /*
	bit_info含义(32bit)
	|----------------|--------|----|----|
	| ts low 16 bit  |m low 8 |frms|idx |
   */
  vl_uint32 bit_info;
  vl_frame_type frm_type;
} vl_audio_frame;


