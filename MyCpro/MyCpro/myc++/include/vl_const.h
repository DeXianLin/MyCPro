#ifndef __VL_CONST_H__
#define __VL_CONST_H__


#ifndef NULL
#define NULL                ((void*)0)
#endif

//#define VL_BIG_ENDIAN         (0)

#define VL_SUCCESS            (0)
#define VL_FAILED             (-1)

#define VL_MAX_PATH           (256)

/**
Error code const define.
Each sub module has it's own error code range, please make sure that error code not in same range.
 */
#define VL_ERR_CODEC_START           (-20000)
#define VL_ERR_CODEC_EFAILED         (VL_ERR_CODEC_START - 1)
#define VL_ERR_CODEC_ENC_NOT_READY   (VL_ERR_CODEC_START - 2)
#define VL_ERR_CODEC_DEC_NOT_READY   (VL_ERR_CODEC_START - 3)
#define VL_ERR_CODEC_ENC_ERROR       (VL_ERR_CODEC_START - 4)
#define VL_ERR_CODEC_DEC_ERROR       (VL_ERR_CODEC_START - 5)
#define VL_ERR_CODEC_PARSE_ERROR     (VL_ERR_CODEC_START - 6)
#define VL_ERR_CODEC_NO_INSTANC      (VL_ERR_CODEC_START - 7)
#define VL_ERR_CODEC_INVALID_PARAM   (VL_ERR_CODEC_START - 8)
#define VL_ERR_CODEC_NO_INIT         (VL_ERR_CODEC_START - 9)
#define VL_ERR_CODEC_ENC_NOT_ENOUGH  (VL_ERR_CODEC_START - 10)

/**
Error code const define.
For audio device on android platform
 */
#define VL_ERR_AUDDEV_START          (-19800)
#define VL_ERR_AUDDEV_PLY_INIT       (VL_ERR_AUDDEV_START - 1)
#define VL_ERR_AUDDEV_REC_INIT       (VL_ERR_AUDDEV_START - 2)
#define VL_ERR_AUDDEV_PLY_NOT_READY  (VL_ERR_AUDDEV_START - 3)
#define VL_ERR_AUDDEV_REC_NOT_READY  (VL_ERR_AUDDEV_START - 4)
#define VL_ERR_AUDDEV_STAT_PLY_FAIL  (VL_ERR_AUDDEV_START - 5)
#define VL_ERR_AUDDEV_STAT_REC_FAIL  (VL_ERR_AUDDEV_START - 6)
#define VL_ERR_AUDDEV_PLY_OP         (VL_ERR_AUDDEV_START - 7)
#define VL_ERR_AUDDEV_REC_OP         (VL_ERR_AUDDEV_START - 8)
#define VL_ERR_AUDDEV_INITAL         (VL_ERR_AUDDEV_START - 9)
#define VL_ERR_AUDDEV_PLY_OCCUPIED   (VL_ERR_AUDDEV_START - 10)
#define VL_ERR_AUDDEV_REC_OCCUPIED   (VL_ERR_AUDDEV_START - 11)
#define VL_ERR_AUDDEV_ID             (VL_ERR_AUDDEV_START - 12)


/**
Error code const define.
For circle buffer.
 */
#define VL_ERR_CCB_START            (-19600)
#define VL_ERR_CCB_EMPTY_READ       (VL_ERR_CCB_START - 1)
#define VL_ERR_CCB_FULL_WRITE       (VL_ERR_CCB_START - 2)
#define VL_ERR_CCB_OP_FAILED        (VL_ERR_CCB_START - 3)
#define VL_ERR_CCB_LOCKED           (VL_ERR_CCB_START - 4)


/**
 * Error code for RTP protocol.
 */
#define VL_ERR_RTP_START                       (-19400)
#define VL_ERR_RTP_PACKET_EXTERNALBUFFERNULL   (VL_ERR_RTP_START - 1)
#define VL_ERR_RTP_PACKET_ILLEGALBUFFERSIZE    (VL_ERR_RTP_START - 2)
#define VL_ERR_RTP_PACKET_INVALIDPACKET        (VL_ERR_RTP_START - 3)
#define VL_ERR_RTP_PACKET_TOOMANYCSRCS         (VL_ERR_RTP_START - 4)
#define VL_ERR_RTP_PACKET_BADPAYLOADTYPE       (VL_ERR_RTP_START - 5)
#define VL_ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE   (VL_ERR_RTP_START - 6)
#define VL_ERR_RTP_OUTOFMEM                    (VL_ERR_RTP_START - 7)
#define VL_ERR_RTP_ERROR                       (VL_ERR_RTP_START - 8)
#define VL_ERR_RTP_OBJ_NULL                    (VL_ERR_RTP_START - 9)
#define VL_ERR_RTP_OBJ_INVALID                 (VL_ERR_RTP_START - 10)

/**
 * Error for IOQ
 */
#define VL_ERR_IOQ_START                       (-19200)
#define VL_ERR_IOQ_CLOSE_WAITING               (VL_ERR_IOQ_START - 1)
#define VL_ERR_IOQ_CLOSE_TIMEOUT               (VL_ERR_IOQ_START - 2)
#define VL_ERR_IOQ_CLOSED                      (VL_ERR_IOQ_START - 3)
#define VL_ERR_IOQ_NO_DATA                     (VL_ERR_IOQ_START - 4)
#define VL_ERR_IOQ_WRITE_LAST_PACKET           (VL_ERR_IOQ_START - 5)
#define VL_ERR_IOQ_CANT_ACCEPT                 (VL_ERR_IOQ_START - 6)
#define VL_ERR_IOQ_STATE                       (VL_ERR_IOQ_START - 7)


/**
 * Error for timer
 */
#define VL_ERR_TIMER_START                     (-19000)
#define VL_ERR_TIMER_CREATE_FAILED             (VL_ERR_TIMER_START - 1)
#define VL_ERR_TIMER_RINNING                   (VL_ERR_TIMER_START - 2)
#define VL_ERR_TIMER_SETTIME                   (VL_ERR_TIMER_START - 3)


/**
 * Error for socket
 */
#define VL_ERR_SOCK_START                     (-18800)
#define VL_ERR_SOCK_NOT_OPEND                 (VL_ERR_SOCK_START - 1)
#define VL_ERR_SOCK_SETOPT                    (VL_ERR_SOCK_START - 2)
#define VL_ERR_SOCK_GETOPT                    (VL_ERR_SOCK_START - 3)
#define VL_ERR_SOCK_BIND                      (VL_ERR_SOCK_START - 4)


/**
 * Error for endpoint
 */
#define VL_ERR_EPT_START                     (-18600)
#define VL_ERR_EPT_INIT_FAILED               (VL_ERR_EPT_START - 1)
#define VL_ERR_EPT_REG_IOQ_UNINIT            (VL_ERR_EPT_START - 2)
#define VL_ERR_EPT_REG_INVALID               (VL_ERR_EPT_START - 3)
#define VL_ERR_EPT_SEND_FAILED               (VL_ERR_EPT_START - 4)
#define VL_ERR_EPT_SEND_INVALID              (VL_ERR_EPT_START - 5)
#define VL_ERR_EPT_TRANS_NODATA              (VL_ERR_EPT_START - 6)
#define VL_ERR_EPT_PROTO_INVALID             (VL_ERR_EPT_START - 7)


/**
 * Error for endpoint codec
 */
#define VL_ERR_PROTO_START                   (-18400)
#define VL_ERR_PROTO_UNENCODED               (VL_ERR_PROTO_START - 1)
#define VL_ERR_PROTO_UNSUPPORT               (VL_ERR_PROTO_START - 2)


/**
 * Error for transaction
 */
#define VL_ERR_TRANS_START                   (-18200)
#define VL_ERR_TRANS_INIT                    (VL_ERR_TRANS_START - 1)
#define VL_ERR_TRANS_PREPARE                 (VL_ERR_TRANS_START - 2)
#define VL_ERR_TRANS_RAISE                   (VL_ERR_TRANS_START - 3)
#define VL_ERR_TRANS_STATE                   (VL_ERR_TRANS_START - 4)
#define VL_ERR_TRANS_PARAM                   (VL_ERR_TRANS_START - 5)


/**
 * Error for silk formated file operation
 */
#define VL_ERR_SILK_STORAGE_START            (-18000)
#define VL_ERR_SILK_STORAGE_OP_INVALID       (VL_ERR_SILK_STORAGE_START - 1)
#define VL_ERR_SILK_STORAGE_OP_FAILED        (VL_ERR_SILK_STORAGE_START - 2)
#define VL_ERR_SILK_STORAGE_PREPARE_FAILED   (VL_ERR_SILK_STORAGE_START - 3)
#define VL_ERR_SILK_STORAGE_CANT_PLAY        (VL_ERR_SILK_STORAGE_START - 4)



#endif
