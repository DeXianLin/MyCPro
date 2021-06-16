
#include "vl_types.h"
#include "vl_const.h"
#include "CodecManager.hpp"

const char* CODEC_CONF_PATH = "/data/data/com.veclink.movnow.conference/files/audio_codec.conf";

static struct AudioEncoderInfo gEncoderInfos[] = {
    {RTP_PT_SILK_NB,SilkEncoder::name, NARROW_BAND, {5000, 10000, 20000}, 3},          /* 0 */
    {RTP_PT_SILK_MB,SilkEncoder::name, MEDIUM_BAND, {7000, 13000, 25000}, 3},          /* 1 */
    {RTP_PT_SILK_WB,SilkEncoder::name, WIDE_BAND, {8000, 19000, 30000}, 3},            /* 2 */
    {RTP_PT_SILK_SWB,SilkEncoder::name, SUPER_WIDE_BAND, {20000, 30000, 40000}, 3},    /* 3 */
};

static const vl_uint8 DEF_PT_IDX = 2;
static const vl_uint8 DEF_BR_IDX = 0;
static const vl_uint8 AUDENC_CONF_COUNT = sizeof(gEncoderInfos) / sizeof(gEncoderInfos[0]);

vl_uint8 CodecManager::defaultPtIdx = DEF_PT_IDX;
vl_uint8 CodecManager::defaultBrIdx = DEF_BR_IDX;

EAUDIO_FORMAT CodecManager::defFormat = AUDIO_FORMAT_SILK;
static const EAUDIO_FORMAT DEF_FORMAT = AUDIO_FORMAT_SILK;

static vl_bool checkIdxValid(vl_uint8 ptIdx, vl_uint8 brIdx) {
    if((ptIdx >= 0 && ptIdx < AUDENC_CONF_COUNT) && (brIdx >= 0 && brIdx < gEncoderInfos[ptIdx].brArraySize)) {
        return VL_TRUE;
    } else {
        return VL_FALSE;
    }
}

/**
 * buildin default parameter for silk and evrc
 */
static AudioEncoderParam defaultSilkParam = {8000,12000,8000 / 5,VL_FALSE,VL_FALSE,VL_FALSE};


static AudioEncoderParam defaultEvrcParam = {8000,4000,8000 / 5,VL_FALSE, VL_FALSE,VL_FALSE};

struct AudioCodecConf {
    EAUDIO_FORMAT conf_format;
};

void CodecManager::setDefaultEncoder(EAUDIO_FORMAT format) {
    defFormat = format;

    FILE* file = fopen(CODEC_CONF_PATH, "w");
    if(file != NULL) {
        struct AudioCodecConf conf;
        conf.conf_format = defFormat;
        size_t written = fwrite(&conf, sizeof(struct AudioCodecConf), 1, file);

        printf("CodecManager write default encoder=%d", defFormat);
        fclose(file);
    }
}

EAUDIO_FORMAT CodecManager::getDefaultEncoder() {
    if(AUDIO_FORMAT_UNKNOWN == defFormat) {
        vl_bool covered = VL_FALSE;
        FILE* file = fopen(CODEC_CONF_PATH, "r");
        if(file != NULL) {
            struct AudioCodecConf conf;
            if(1 == fread(&conf, sizeof(struct AudioCodecConf), 1, file)) {
                printf("CodecManager getDefaultEncoder config readed !!!");
                covered = VL_TRUE;
                defFormat = conf.conf_format;
            }
            fclose(file);
        } else {
            printf("CodecManager get default encoder,config file not exist.");
        }
        if(VL_FALSE == covered) {
            defFormat = DEF_FORMAT;
        }
    }

    return defFormat;
}

void CodecManager::setDefaultEncoderComplex(int format) {
    printf("CodecManager set complex format code %d", format);

    defFormat = (EAUDIO_FORMAT)(format >> 16);
    int bitrate = format & 0xFFFF;

    printf("CodecManager set complex format code %d, real format=%d bitrate=%d", format, defFormat, bitrate);

    FILE* file = fopen(CODEC_CONF_PATH, "w");
    if(file != NULL) {
        struct AudioCodecConf conf;
        conf.conf_format = defFormat;
        size_t written = fwrite(&conf, sizeof(struct AudioCodecConf), 1, file);
        printf("CodecManager write default encoder=%d", defFormat);
        fclose(file);
    }

    if (defFormat == AUDIO_FORMAT_SILK) {
        defaultSilkParam.bitrate = bitrate;
        printf("CodecManager silk bitrate set to %d", defaultSilkParam.bitrate);
    } else if (defFormat == AUDIO_FORMAT_EVRC) {
        defaultEvrcParam.bitrate = bitrate;
        printf("CodecManager evrc bitrate set to %d", defaultEvrcParam.bitrate);
    }
}

int CodecManager::getDefaultEncoderComplex() { 
  if(AUDIO_FORMAT_UNKNOWN == defFormat) {
        vl_bool covered = VL_FALSE;
        FILE* file = fopen(CODEC_CONF_PATH, "r");
        if(file != NULL) {
            struct AudioCodecConf conf;
            if(1 == fread(&conf, sizeof(struct AudioCodecConf), 1, file)) {
                printf("CodecManager getDefaultEncoder config readed !!!");
                covered = VL_TRUE;
                defFormat = conf.conf_format;
            }
            fclose(file);
        } else {
            printf("CodecManager get default encoder,config file not exist.");
        }
        if(VL_FALSE == covered) {
            defFormat = DEF_FORMAT;
        }
    }

    int complexCode = (int) defFormat;

    if (defFormat == AUDIO_FORMAT_SILK) {
        return (complexCode << 16) | defaultSilkParam.bitrate;
    } else if (defFormat == AUDIO_FORMAT_EVRC) {
        return (complexCode << 16) | defaultEvrcParam.bitrate;
    } else {
        return 0;
    }
}

AudioEncoder* CodecManager::createAudioEncoder(EAUDIO_FORMAT format) {
    AudioEncoder* encoder = NULL;

    if(AUDIO_FORMAT_UNKNOWN == format) {
        printf("CodecManager aquire encoder format type =%d", format);
        format = getDefaultEncoder();
    }

    if(AUDIO_FORMAT_SILK == format) {
        encoder = new SilkEncoder(defaultSilkParam);
        printf("CodecManager create silk encode with default param.");
    }

    return encoder;
} 

AudioEncoder* CodecManager::createAudioEncoder(EAUDIO_FORMAT format, const AudioEncoderParam& param) {
    AudioEncoder* encoder = NULL;
    if(AUDIO_FORMAT_SILK == format) {
        encoder = new SilkEncoder(param);
        printf("CodecManager create silk encode, sr=%d, br=%d, samperpack=%d", param.sampleRate, param.bitrate, param.packageSampleCount);
    }

    return encoder;
}






AudioDecoder* CodecManager::createAudioDecoder(ERTP_PT payloadType)
{
    int sampleRate = PayloadTypeMapper::getSampleRateByPT(payloadType);
    EAUDIO_FORMAT format = PayloadTypeMapper::getCodecByPT(payloadType);

    if(sampleRate <= 0 || AUDIO_FORMAT_UNKNOWN == format) {
        printf("CodecManager create audio decoder failed with invalid payload type : %d", payloadType);
        return NULL;
    }

    AudioDecoderParam param;
    param.sampleRate = sampleRate;
    param.plc = VL_FALSE;
    return createAudioDecoder(format, param);
}

AudioDecoder* CodecManager::createAudioDecoder(EAUDIO_FORMAT format, const AudioDecoderParam& param) {
    AudioDecoder* decoder = NULL;

    switch(format) {
    case AUDIO_FORMAT_SILK:
        decoder = new SilkDecoder(param);
        break;
    case AUDIO_FORMAT_EVRC:
        //decoder = new EvrcDecoder(param);
        break;
    default:
        printf("CodecManager create audio decoder with unknown format");
        break;
    }
    return decoder;
}



















