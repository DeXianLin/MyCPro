#ifndef __VL_TYPES_H__
#define __VL_TYPES_H__
//#include <printf>
//#include <WinSock2.h>
#include <sys/socket.h>
#include<netinet/in.h>


/** Signed 32bit integer. */
typedef int     vl_int32;

/** Unsigned 32bit integer. */
typedef unsigned int    vl_uint32;

/** Signed 16bit integer. */
typedef short       vl_int16;

/** Unsigned 16bit integer. */
typedef unsigned short  vl_uint16;

/** Signed 8bit integer. */
typedef signed char vl_int8;

/** Unsigned 8bit integer. */
typedef unsigned char vl_uint8;

/** Boolean. */
typedef int     vl_bool;

enum vl_boolean {
    VL_TRUE = 1,
    VL_FALSE = 0
};

typedef int vl_status;

typedef long long vl_int64;

typedef unsigned long long vl_uint64;

typedef unsigned int vl_size;

typedef long long vl_timestamp;

typedef signed char vl_char;

typedef unsigned int vl_offset;

typedef struct sockaddr vl_sockaddr;

/*typedef struct {
    vl_uint32 addr;
    vl_uint16 port;
} vl_sockaddr_in;*/


/** Socket handle. */
typedef long vl_sock;

/** Generic socket address. */
typedef void vl_sockaddr_t;


/** Large unsigned integer. */
typedef unsigned long  vl_size_t;

/** Large signed integer. */
typedef long  vl_ssize_t;


/* ************************************************************************* */
/*
 * Data structure types.
 */
/**
 * This type is used as replacement to legacy C string, and used throughout
 * the library. By convention, the string is NOT null terminated.
 */
typedef struct vl_str_t {
    /** Buffer pointer, which is by convention NOT null terminated. */
    char       *ptr;

    /** The length of the string. */
    vl_ssize_t  slen;
} vl_str_t;


/**
 * Representation of time value in this library.
 * This type can be used to represent either an interval or a specific time
 * or date. 
 */
typedef struct vl_time_val
{
    /** The seconds part of the time. */
    long    sec;

    /** The miliseconds fraction of the time. */
    long    msec;

} vl_time_val;


typedef void LOCK_OBJ;

/*
 * Lock structure.
 */
struct vl_lock_t
{
    LOCK_OBJ *lock_object;

    vl_status	(*acquire)	(LOCK_OBJ*);
    vl_status	(*tryacquire)	(LOCK_OBJ*);
    vl_status	(*release)	(LOCK_OBJ*);
    vl_status	(*destroy)	(LOCK_OBJ*);
};

/** Thread handle. */
typedef struct vl_thread_t vl_thread_t;

/** Lock object. */
typedef struct vl_lock_t vl_lock_t;




#endif















