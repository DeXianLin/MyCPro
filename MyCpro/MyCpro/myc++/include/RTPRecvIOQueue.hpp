#ifndef __RTP_RECV_IOQUEUE_HPP__
#define __RTP_RECV_IOQUEUE_HPP__

#include "IOQueue.hpp"
#include "RTPJitterBuffer.hpp"
#include "StreamPlaySource.hpp"

//#include "com_veclink_controller_media_MediaOpJNI.h"

class RTPRecvIOQueue : public IOQueue {
public:
    ~RTPRecvIOQueue(){
        if(NULL != jitter) {
            delete jitter;
        }
    }
    RTPRecvIOQueue(EProtocol proto, vl_uint32 rip, vl_uint16 rport) : IOQueue(proto, rip, rport), jitter(NULL) {initial();}
    RTPRecvIOQueue(EProtocol proto, const char * rip, vl_uint16 rport) : IOQueue(proto, rip, rport), jitter(NULL) {initial();}
    RTPRecvIOQueue(EProtocol proto, SockAddr& remoteAddr) : IOQueue(proto, remoteAddr) , jitter(NULL) {initial();}

    void onReadEmpty() {
        jitter->rehold();
        /* 播放过程中无数据可读，说明卡住一次 */
        if(VL_TRUE == playing) {
            summary.block_times ++;
            playing = VL_FALSE;
        }
    }

    vl_int32 getPredictContinuousMS() const {
        if(NULL == jitter) {
            return 0;
        }
    return jitter->getPredictContinousMS();
    }


    void onJitterReceive() {
        //printf("RTPRecvIOQueue recev !!!!!");
        notify();
    }

    virtual vl_status write(Transmittable *data) {
        if (VL_TRUE == canAccept(data))
        {
            //printf("RTPRecvIOQueue receive a packet");
            if(NULL != data) {
                RTPObject* rtpObj = dynamic_cast<RTPObject*>(data->getObject());
                uint64_t curr_time_ms = getTimeStampMS();

                if(VL_TRUE == checkPayloadType((ERTP_PT)rtpObj->getPayloadType())) {
                    payloadType = (ERTP_PT)rtpObj->getPayloadType();
                    //printf("RTPRecvIOQueue update payload type : %d", payloadType);
                }

                totalCount++;

                if(totalCount > 1) {
                    uint32_t interval = rtpObj->getTimestamp() - lastSendMs;
                    int32_t currentJitter = (curr_time_ms - lastRecvMs) - interval;

                    if(abs(currentJitter) > abs(maxJitter)) {
                        maxJitter = currentJitter;
                    }

                    totalJitter += currentJitter;
                }

                lastSendMs = rtpObj->getTimestamp();
                lastRecvMs = curr_time_ms;

                if(0 == summary.first_recv_ts) {
                    summary.first_recv_ts = curr_time_ms;
                    summary.ssrc = rtpObj->getSSRC();

                    ExternData* extData = (ExternData*)rtpObj->getExtensionData();
                    if(extData != NULL) {
                        summary.send_id = ntohl(extData->send_id);
                        summary.flag = (uint16_t)extData->flag;

                        if(0 == extData->flag) {
                            summary.group_id = 0;
                        } else {
                            summary.group_id = ntohl(extData->recv_id);
                        }
                        summary.send_os = extData->os_type;
                        summary.send_net_type = extData->net_type;
                    }
                }

                if(rtpObj->getSequenceNumber() > summary.max_seq) {
                    summary.max_seq = rtpObj->getSequenceNumber();
                }

                //		LOGW("RTPRecvIOQueue recvei rtp object seq = %d, hasMarker = %d", rtpObj->getSequenceNumber(), rtpObj->hasMarker());
                jitter->push(data);
            }


            return VL_SUCCESS;
    }
        return VL_FAILED;
    }

    void updateAndGetSummary(struct RecvTransInfo& dstSummary) {
        updateRecvSummary();

        dstSummary.flag = summary.flag;
        dstSummary.ssrc = summary.ssrc;
        dstSummary.recv_id = summary.recv_id;
        dstSummary.group_id = summary.group_id;
        dstSummary.send_id = summary.send_id;
        dstSummary.max_seq = summary.max_seq;
        dstSummary.dur_per_pack = summary.dur_per_pack;
        dstSummary.normalEnd = summary.normalEnd;
        dstSummary.max_jitter = summary.max_jitter;
        dstSummary.total_jitter = summary.total_jitter;
        dstSummary.block_times = summary.block_times;
        dstSummary.lost_persent = summary.lost_persent;
        dstSummary.first_recv_ts = summary.first_recv_ts;
        dstSummary.send_net_type = summary.send_net_type;
        dstSummary.send_os = summary.send_os;
    }

    Transmittable* read() {
        Transmittable* trans = this->IOQueue::read();
        if(VL_FALSE == playing && trans != NULL) {
            /* 有数据可以读出时，表明播放进行中 */
            playing = VL_TRUE;
        }
        return trans;
    }

    ERTP_PT getPayloadType() {
        return this->payloadType;
    }

private:
    void updateRecvSummary() {
        if(0 == summary.dur_per_pack) {
            summary.dur_per_pack = jitter->rtpDuration;
        }

        summary.max_jitter = maxJitter;
        summary.total_jitter = totalJitter;
        summary.lost_persent = jitter->getLostPercent();
        summary.normalEnd = jitter->isEnded;
    }

    void initial() {
        if(NULL != jitter) {
            delete jitter;
        }
        jitter = new RTPJitterBuffer(this);
        totalCount = 0;

        playing = VL_FALSE;
        maxJitter = 0;
        totalJitter = 0;

        payloadType = RTP_PT_UNKNOWN;

        memset(&this->summary, 0, sizeof(struct RecvTransInfo));
    }
    RTPJitterBuffer* jitter;
    uint32_t totalCount;

    vl_bool playing;
    int32_t maxJitter;
    int32_t totalJitter;


    uint32_t lastSendMs; // rtp ms 负载，高位截断
    uint64_t lastRecvMs; // 最后一次接收rtp本地时间戳

    ERTP_PT payloadType;

    struct RecvTransInfo summary;
};

#endif
