


#define VL_SUCCESS            (0)
#define VL_MAX_PATH           (256)

/*
Error code const define.
Each sub module has it's own error code range, please make sure that error code not in same range.
 */
#define VL_ERR_CODEC_START           (-10200)
#define VL_ERR_CODEC_EFAILED         (VL_ERR_CODEC_START + 1)
#define VL_ERR_CODEC_ENC_NOT_READY   (VL_ERR_CODEC_START + 2)
#define VL_ERR_CODEC_DEC_NOT_READY   (VL_ERR_CODEC_START + 3)
#define VL_ERR_CODEC_ENC_ERROR       (VL_ERR_CODEC_START + 4)
#define VL_ERR_CODEC_DEC_ERROR       (VL_ERR_CODEC_START + 5)
#define VL_ERR_CODEC_PARSE_ERROR     (VL_ERR_CODEC_START + 6)

/*
Error code const define.
For audio device on android platform
 */
#define VL_ERR_AUDDEV_START          (-10400)
#define VL_ERR_AUDDEV_PLY_INIT       (VL_ERR_AUDDEV_START + 1)
#define VL_ERR_AUDDEV_REC_INIT       (VL_ERR_AUDDEV_START + 2)
#define VL_ERR_AUDDEV_PLY_NOT_READY  (VL_ERR_AUDDEV_START + 3)
#define VL_ERR_AUDDEV_REC_NOT_READY  (VL_ERR_AUDDEV_START + 4)
#define VL_ERR_AUDDEV_STAT_PLY_FAIL  (VL_ERR_AUDDEV_START + 5)
#define VL_ERR_AUDDEV_STAT_REC_FAIL  (VL_ERR_AUDDEV_START + 6)
#define VL_ERR_AUDDEV_PLY_OP         (VL_ERR_AUDDEV_START + 7)
#define VL_ERR_AUDDEV_REC_OP         (VL_ERR_AUDDEV_START + 8)
#define VL_ERR_AUDDEV_INITAL         (VL_ERR_AUDDEV_START + 9)
#define VL_ERR_AUDDEV_PLY_OCCUPIED   (VL_ERR_AUDDEV_START + 10)
#define VL_ERR_AUDDEV_REC_OCCUPIED   (VL_ERR_AUDDEV_START + 11)
#define VL_ERR_AUDDEV_ID             (VL_ERR_AUDDEV_START + 12)


/*
Error code const define.
For circle buffer.
 */
#define VL_ERR_CCB_START            (-10600)
#define VL_ERR_CCB_EMPTY_READ       (VL_ERR_CCB_START + 1)
#define VL_ERR_CCB_FULL_WRITE       (VL_ERR_CCB_START + 2)
#define VL_ERR_CCB_OP_FAILED        (VL_ERR_CCB_START + 3)
#define VL_ERR_CCB_LOCKED           (VL_ERR_CCB_START + 4)
