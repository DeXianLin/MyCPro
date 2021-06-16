#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "SilkFile.hpp"
#include "vl_const.h"

static const char* TAG = "SilkStorage";
const char * FSILK_HEADER = "#!SILK\n";
const char * FEVRC_HEADER = "#!EVRC\n"; 

SilkStorage::SilkStorage(const char* path, vl_bool isRecord) :
  isRecord(isRecord), format(AUDIO_FORMAT_UNKNOWN)
{

    // 打开文件用于保存/读取音频文件
    if(VL_TRUE == isRecord)
    {
        file = fopen(path, "ab+");
    } else
    {
        file = fopen(path, "rb");

        //判断文件是否为silk音频文件
        if(NULL != file)
        {
            char header[20];
            memset(header, 0, sizeof(header));
            fseek(file, 0, SEEK_SET);
            fread(header, sizeof(header), 1, file);

            printf("AudioFile head[20]=%s", header);

            if(header == strstr(header, FSILK_HEADER))
            {
                format = AUDIO_FORMAT_SILK;
                printf("AudioStorage open silk file.");
                fseek(file, strlen(FSILK_HEADER), SEEK_SET);
            } else if(header == strstr(header, FEVRC_HEADER)) {
                format = AUDIO_FORMAT_EVRC;
                printf("AudioStorage open evrc file.");
                fseek(file, strlen(FSILK_HEADER), SEEK_SET);
            } else {
                printf("open file %s for read is not a silk format file", path);
                fclose(file);
                file = NULL;
            }
        }
    }

    if(NULL == file) {
        //printf("open silk file %s failed !!", path);
    }

}

SilkStorage::~SilkStorage() {
    if(NULL != file && VL_TRUE == isRecord)
    {
        printf("Silk storage file saved !!!!");
        fflush(file);
        fclose(file);
        file = NULL;
    }
}

vl_status SilkStorage::readBlock(SilkBlock& block) {
    if(VL_FALSE == readable()) {
        printf("read block from unreadable file");
        return VL_ERR_SILK_STORAGE_OP_FAILED;
    }

    SilkBlockHeader header;
    vl_int8* payload = NULL;

    do {
        size_t rret;
        rret = fread((void*)&header, sizeof(header), 1, file);

        if(1 != rret) {
            printf("%s read block head failed, result = %d", TAG, rret);
      break;
        }

        if(header.getMode() > ESILK_BLOCK_MAX) {
      printf("%s read block mode invalid", TAG);
            break;
        }

        if(header.getOctets() == 0) {
            printf("%s read block octets = 0", TAG);
            break;
        }

        printf("header . octets = %d", header.getOctets());

        block.length = header.getOctets();
        block.spr = (ESILKBLOCK) header.getMode();
        block.timestamp = header.getTimestamp();

        payload = (vl_int8*)malloc(header.getOctets());

        if(payload == NULL) {
            printf("%s read block malloc failed", TAG);
            break;
        }

        if(1 != fread(payload, block.length, 1, file)) {
            printf("%s read block, read payload failed", TAG);
            break;
        }


        block.payload = payload;

        return VL_SUCCESS;
    } while(0);

    if(NULL != payload) {
        free(payload);
        payload = NULL;
    }
    return VL_ERR_SILK_STORAGE_OP_FAILED;
}

vl_status SilkStorage::writeBlock(const SilkBlock& block)
{
    if(VL_FALSE == writtable())
    {
        //printf("write block from unreadable file");
        return VL_ERR_SILK_STORAGE_OP_FAILED;
    }

    /* 检查传入参数的有效性 */
    if(block.spr > ESILK_BLOCK_MAX || block.payload == NULL || block.length == 0)
    {
        printf("%s write block failed, invalid parameter, spr=%d, payload=%p, len=%d", TAG, block.spr, block.payload, block.length);
        return VL_ERR_SILK_STORAGE_OP_FAILED;
    }

    SilkBlockHeader header;
    header.setFlag(block.spr, block.length);
    header.setTimestamp(block.timestamp);


    do {
        size_t ret;
        /* 第一个block要写入魔数 */
        if(0 == ftell(file))
        {
            const char* magic = getMagicStr();
            if(NULL == magic) {
                printf("%s unknow audio fomat, can't write magic", TAG);
                break;
            } else {
                printf("%s audio file magic is %s", TAG,(char*)magic);
            }
            ret =  fwrite(magic, strlen(magic), 1, file);
            if(1 != ret) {
                printf("%s write block, write file header file failed", TAG);
                break;
            }
        }

        ret = fwrite(&header, sizeof(header), 1, file);
        if(1 != ret) {
            printf("%s write block, write block header failed, ret=%d, expect size = %d", TAG, ret, sizeof(header));
            break;
        }

        ret = fwrite(block.payload, block.length, 1, file);
        if(1 != ret) {
            printf("%s write block, write payload to file failed", TAG);
            break;
        }

        return VL_SUCCESS;
    } while(0);

    return VL_ERR_SILK_STORAGE_OP_FAILED;
}

vl_bool SilkStorage::readable() const { 
  if(NULL != file && VL_FALSE == isRecord) {
        return VL_TRUE;
    }
  return VL_FALSE;
}

vl_bool SilkStorage::writtable() const { 
  if(NULL != file && VL_TRUE == isRecord) { 
    return VL_TRUE;
    }
  return VL_FALSE;
}

vl_bool SilkStorage::setFormat(EAUDIO_FORMAT fmt) {
    if(VL_TRUE == isRecord && VL_TRUE == PayloadTypeMapper::isValidAudioFormat(fmt))
    {
        this->format = fmt;
        return VL_TRUE;
    }
    return VL_FALSE;
}

vl_bool SilkStorage::resetOffset() {
    if(VL_FALSE == PayloadTypeMapper::isValidAudioFormat(format)) {
        return VL_FALSE;
    }

    if(readable() || writtable()) {
        if(0 == fseek(file, strlen(getMagicStr()), SEEK_SET)) {
            return VL_TRUE;
        }
    }
  return VL_FALSE;
}

const char* SilkStorage::getMagicStr() {
    const char* str = NULL;
    switch (this->format) {
    case AUDIO_FORMAT_SILK:
        str = FSILK_HEADER;
        break;
    case AUDIO_FORMAT_EVRC:
        str = FEVRC_HEADER;
        break;
    default:
        break;
    }
    return str;
}

