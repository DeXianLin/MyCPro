
#ifndef __VL_TYPES_H__
#define __VL_TYPES_H__
#include <printf>
/** Signed 32bit integer. */
typedef int		vl_int32;

/** Unsigned 32bit integer. */
typedef unsigned int	vl_uint32;

/** Signed 16bit integer. */
typedef short		vl_int16;

/** Unsigned 16bit integer. */
typedef unsigned short	vl_uint16;

/** Signed 8bit integer. */
typedef signed char	vl_int8;

/** Unsigned 8bit integer. */
typedef unsigned char vl_uint8;

/** Boolean. */
typedef int		vl_bool;

enum vl_boolean
{
  VL_FALSE = 0,
  VL_TRUE = 1
};

typedef int vl_status;

typedef long long vl_int64;

typedef unsigned long long vl_uint64;

typedef unsigned int vl_size;

typedef long long vl_timestamp;

typedef signed char vl_char;

typedef unsigned int vl_offset;

typedef struct {
  vl_uint32 addr;
  vl_uint16 port;
} vl_sockaddr_in;

#endif















