#include "vl_types.h"
#include "vl_const.h"
#include "RTPProtocol.hpp"

static vl_uint32 getRTPPayloadOffset(vl_uint8* packet, vl_size pktLen) {
    RTPHeader *rtpheader = (RTPHeader *)packet;
    vl_uint32 payloadoffset =
        sizeof(RTPHeader) + (int)(rtpheader->csrccount * sizeof(vl_uint32));

    if (0 != rtpheader->extension) {
        RTPExtensionHeader *rtpextheader =
            (RTPExtensionHeader *)(packet + payloadoffset);
        payloadoffset += sizeof(RTPExtensionHeader);
        payloadoffset += ntohs(rtpextheader->length) * sizeof(vl_uint32);
    }
    return payloadoffset;
}

static vl_uint32 getRTPPaddingSize(vl_uint8* packet, vl_size pktlen) {
    RTPHeader *rtpheader = (RTPHeader *)packet;
    vl_uint32 numpadbytes = 0;

    if (rtpheader->padding) {
        numpadbytes = (int)packet[pktlen - 1];
    }
    return numpadbytes;
}

vl_status RTPProtocol::encode(ProtoObject *obj, void *extbuf, vl_size *extbuflen) {
    vl_uint8* destptr;
    vl_size capacity;

    if(NULL != extbuf && NULL != extbuflen && *extbuflen > 0) {
        destptr = (vl_uint8*)extbuf;
        capacity = *extbuflen;
    } else {
        destptr = (vl_uint8*)obj->getPacket();
        capacity = obj->getPacketCapacity();
    }

    if(NULL == destptr || capacity < sizeof(RTPHeader)) {
        printf("rtp encode failed, extpacket=%p, extpktlen=%d, packet=%p, pktlen=%d",
               (vl_uint8*)extbuf, *extbuflen, (vl_uint8*)obj->getPacket(), obj->getPacketCapacity());

        return VL_ERR_RTP_PACKET_ILLEGALBUFFERSIZE;
    }

    if(NULL == obj) {
        printf("rtp encode failed, obj is null");
        return VL_ERR_RTP_OBJ_NULL;
    }

    RTPObject* rtpObj = dynamic_cast<RTPObject*>(obj);
    if(VL_TRUE != rtpObj->isDecoded()) {
        printf("rtp encode failed, obj is not decoded");
        return VL_ERR_RTP_OBJ_INVALID;
    }

    if (rtpObj->getCSRCCount() > RTP_MAXCSRCS)
        return VL_ERR_RTP_PACKET_TOOMANYCSRCS;

    if (rtpObj->getPayloadType() == 72 || rtpObj->getPayloadType() == 73 || rtpObj->getPayloadType() > 127) {
    // high bit should not be used
        // could cause confusion with rtcp types
        printf("RtpProtocol encode rtp with invalid payloadtype = %d", rtpObj->getPayloadType());
        return VL_ERR_RTP_PACKET_BADPAYLOADTYPE;
    }

    RTPHeader *rtphdr = (RTPHeader*) destptr;
    //  vl_uint8* curptr = destptr + sizeof(RTPHeader);
    vl_uint32 csrcbytes = 0;
    //  vl_uint32 extensionbytes = 0;

    rtphdr->version = RTP_VERSION;
    rtphdr->payloadtype = rtpObj->getPayloadType() & 127;
    rtphdr->ssrc = htonl(rtpObj->getSSRC());
    rtphdr->sequencenumber = htons(rtpObj->getSequenceNumber());
    rtphdr->timestamp = htonl(rtpObj->getTimestamp());
    rtphdr->padding = 0;
    rtphdr->marker = (VL_TRUE == rtpObj->hasMarker()) ? 1 : 0;

    vl_size rtpHeaderLen = sizeof(RTPHeader);

    /* csrcs */
    rtphdr->csrccount = rtpObj->getCSRCCount();
    if(rtpObj->getCSRCCount() > 0) {
        csrcbytes = rtpObj->getCSRCCount() * sizeof(vl_uint32);
        memcpy((void*)(destptr + rtpHeaderLen), (const void*)rtpObj->getCSRCArray(), csrcbytes);
    }

    /* extension */
    if(rtpObj->hasExtension()) {
        rtphdr->extension = 1;
        RTPExtensionHeader *rtpexthdr = (RTPExtensionHeader *)(destptr + sizeof(RTPHeader) + csrcbytes);
        rtpexthdr->extid = htons(rtpObj->getExtensionID());
        rtpexthdr->length = htons((uint16_t)rtpObj->getExtensionLength() / sizeof(uint32_t));

        rtpHeaderLen += sizeof(RTPExtensionHeader);
        memcpy((void*)(destptr + rtpHeaderLen), rtpObj->getExtensionData(), rtpObj->getExtensionLength());
        rtpHeaderLen += rtpObj->getExtensionLength();
    } else {
        rtphdr->extension = 0;
    }

    rtpObj->setEncoded(VL_TRUE, rtpHeaderLen + rtpObj->getPayloadLength());
    return VL_SUCCESS;
}

vl_status RTPProtocol::decode(ProtoObject *obj, void *packet, vl_size pktlen)
{
    if (NULL == obj || NULL == packet || pktlen < sizeof(RTPHeader)) {
        printf("rtp protocol decode failed, obj=%p, packet=%p, len=%d", obj, packet,pktlen);
        return VL_ERR_RTP_PACKET_INVALIDPACKET;
    }

    RTPHeader *rtpHeader = (RTPHeader *)packet;//头部
    RTPObject *rtpObj = dynamic_cast<RTPObject *>(obj);

    vl_uint32 payloadOffset = getRTPPayloadOffset((vl_uint8*)packet, pktlen);
    vl_uint32 paddingbytes = getRTPPaddingSize((vl_uint8*)packet, pktlen);

    rtpObj->setPayloadType(rtpHeader->payloadtype);
    rtpObj->setMark((0 == rtpHeader->marker) ? VL_FALSE : VL_TRUE);
    rtpObj->setSSRC(ntohl(rtpHeader->ssrc));
    rtpObj->setSequence(ntohs(rtpHeader->sequencenumber));
    rtpObj->setTimestamp(ntohl(rtpHeader->timestamp));
    rtpObj->setPayloadByOffset(payloadOffset, pktlen - payloadOffset - paddingbytes);

    if (0 != rtpHeader->extension)
    {
        vl_uint32 extOff = sizeof(RTPHeader) + (int)(rtpHeader->csrccount * sizeof(vl_uint32));
        RTPExtensionHeader *rtpextheader = (RTPExtensionHeader *)(rtpObj->getPacket() + extOff);
        rtpObj->setExtension(rtpextheader->extid, (int)ntohs(rtpextheader->length), extOff + sizeof(RTPExtensionHeader));
    }

    rtpObj->setCSRCs(rtpHeader->csrccount);
    rtpObj->setDecoded(VL_TRUE);
    return VL_SUCCESS;
}

ProtoObject* RTPProtocol::allocObject(void* packet, vl_size pktlen) {
    return new RTPObject((vl_uint8*)packet, pktlen);
}

vl_bool RTPProtocol::parse(void *packet, vl_size pktlen)
{
    vl_bool isValidRTP = VL_FALSE;
    do {
        RTPHeader *rtpheader = (RTPHeader *)packet;
        vl_int32 payloadoffset;
        vl_int32 numpadbytes;

        if (NULL == packet || pktlen < sizeof(RTPHeader))
        {
            printf("rtp protocol parse failed, packet=%p, len=%d", packet, pktlen);
            break;
        }

        if (RTP_VERSION != rtpheader->version) {
            printf("rtp protocol parse failed, rtp version=%d len=%d", rtpheader->version, pktlen);
            break;
        }

        //printf("rtp protocol parse succ, rtp version=%d len=%d", rtpheader->version, pktlen);
        if (rtpheader->marker != 0) {
            if ((rtpheader->payloadtype == (RTP_RTCPTYPE_SR & 127)) ||
                (rtpheader->payloadtype == (RTP_RTCPTYPE_RR & 127)))
            {
                printf("rtp protocol parse failed, rtp marker=%d, pt=%d",
                       rtpheader->marker, rtpheader->payloadtype);
                break;
            }
        }

        payloadoffset = getRTPPayloadOffset((vl_uint8*)packet, pktlen);
        numpadbytes = getRTPPaddingSize((vl_uint8*)packet, pktlen);

        if(payloadoffset < sizeof(RTPHeader)) {
            printf("rtp protocol payloadoffset=%d", payloadoffset);
            break;
        }

        if ((pktlen - numpadbytes - payloadoffset) < 0) {
            printf("rtp protocol parse failed, pktlen=%d, pading=%d, offset=%d", pktlen,
                   numpadbytes, payloadoffset);
            break;
        }
        isValidRTP = VL_TRUE;

        printf("rtp protocol parse succ, pktlen=%d, pading=%d, offset=%d", pktlen,
               numpadbytes, payloadoffset);
    } while (0);

    return isValidRTP;
}

vl_size RTPProtocol::getPrefixSize(void *param) {
    vl_size prefixSize = sizeof(RTPHeader);

    if (NULL != param) {
        RTPEncParam *encParam = (RTPEncParam *)param;

        /* csrc附加大小 */
        if (encParam->csrcnum > 0) {
            prefixSize += (vl_size)(encParam->csrcnum * sizeof(vl_uint32));
        }

        /* 外部附加数据大小 */
        if (encParam->extnum > 0) {
            prefixSize += sizeof(RTPExtensionHeader);
            prefixSize += encParam->extnum * sizeof(vl_uint32);
        }
    }

    return prefixSize;
}

void RTPProtocol::freeObject(ProtoObject* obj) {
    if(obj != NULL)
        delete  (dynamic_cast<RTPObject*>(obj));
}
