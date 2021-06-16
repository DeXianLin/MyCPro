#ifndef __RECORD_FILTER_HPP__
#define __RECORD_FILTER_HPP__
#include "IOFilter.hpp"
#include "SilkFile.hpp"
#include "ProtoObject.hpp"
#include "RTPProtocol.hpp"
#include "AudioInfo.hpp"
#include "AudioCodec.hpp"
#include "MemoryPool.hpp"
#include "CodecManager.hpp"




/**
 * 负责将io队列中的帧数据转到文件中，不包括音频文件的结构化解析和存储。
 */
class RecordFilter : public IOFilter {
public:
    RecordFilter(const char* path, EAUDIO_FORMAT fmt = AUDIO_FORMAT_UNKNOWN, ECODEC_BAND_TYPE bt = UNKNOWN_BAND) :
        silkStorage(path, VL_TRUE), format(fmt), bandType(bt) {}

    void process(Transmittable * data)
    {
        ESILKBLOCK blockType = (ESILKBLOCK)-1;

        do {
            if(NULL == data) {
                printf("RecordFilter process transmittable failed, data is null");
                break;
            }

            if(EPROTO_RTP != data->getProtocol()) {
                printf("RecordFilter process failed, transmittable is not rtp data");
                break;
            }

            RTPObject* rtpObj = dynamic_cast<RTPObject*>(data->getObject());
            if(NULL == rtpObj) {
                printf("RecrodFilter process failed, data object is null");
                break;
            }

            if(VL_TRUE != rtpObj->isDecoded()) {
                printf("RecordFilter process failed, rtpObj is not decoded yet");
                break;
            }

            EAUDIO_FORMAT new_fmt = PayloadTypeMapper::getCodecByPT((ERTP_PT)rtpObj->getPayloadType());
            ECODEC_BAND_TYPE new_bt = PayloadTypeMapper::getBandTypeByPT((ERTP_PT)rtpObj->getPayloadType());

            if(VL_TRUE != PayloadTypeMapper::isValidAudioFormat(new_fmt))
            {
                if(rtpObj->getSequenceNumber() > 5)
                    printf("RecordFilter process failed, audio format is not valid");
                break;
            }

            if(AUDIO_FORMAT_UNKNOWN == format || UNKNOWN_BAND == bandType)
            {
                format = new_fmt;
                bandType = new_bt;
                silkStorage.setFormat(format);
                printf("RecorderFilter will record file, format=%d, bandtype=%d", format, bandType);
            } else if(format != new_fmt || bandType != new_bt) {
                printf("RecorderFilter changed !! old fmt=%d, new fmt=%d; old bt=%d, new bt=%d", format, new_fmt, bandType, new_bt);
                break;
            }
            switch(bandType) {
            case NARROW_BAND:
                blockType = ESILK_BLOCK_8K;
                break;
            case MEDIUM_BAND:
                blockType = ESILK_BLOCK_12K;
                break;
            case WIDE_BAND:
                blockType = ESILK_BLOCK_16K;
                break;
            case SUPER_WIDE_BAND:
                blockType = ESILK_BLOCK_24K;
                break;
            default:
                break;
            }

            if(-1 == blockType) {
                printf("RecordFilter process failed, bandtype not support");
                break;
            }

            vl_uint8* payload = rtpObj->getPayload();
            vl_size payloadlen = rtpObj->getPayloadLength();
            vl_uint32 ts = rtpObj->getTimestamp();
            SilkBlock audioBlock;

            if(VL_TRUE == PayloadTypeMapper::isUseBlockPacket(format))
            {
                AudPacket encPkt((vl_int8*)payload, payloadlen, 0);

                /* 将RTP帧数据写入到文件 */
                while ((audioBlock.length = encPkt.getNextBlock(&audioBlock.payload)) > 0)
                {
                    audioBlock.timestamp = ts;
                    audioBlock.spr = blockType;
                    silkStorage.writeBlock(audioBlock);
                }
      } else {
                audioBlock.spr = blockType;
                audioBlock.length = payloadlen;
                audioBlock.timestamp = ts;
                audioBlock.payload = (vl_int8*)payload;
                silkStorage.writeBlock(audioBlock);
            }
        } while(0);

    }
private:
    SilkStorage silkStorage;
    EAUDIO_FORMAT format;
    ECODEC_BAND_TYPE bandType;
};

#endif




