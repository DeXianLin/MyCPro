#ifndef __RTP_HEART_BEAT_HPP__
#define __RTP_HEART_BEAT_HPP__

#include "vl_const.h"
#include "EptHeartBeat.hpp"

typedef struct {
#if defined(VL_BIG_ENDIAN)  // 大端
    vl_uint8 magic:8;         // 心跳标志， 0x1C
    vl_uint8 ver:4;           // 版本号
    vl_uint8 needRsp:4;       // 0x0 表示不需要回复，0x1表示需要服务器回传外网ip：port
    vl_uint16 reserb1:16;       // 保留
#else  // 小端模式
    vl_uint8 magic:8;         // 心跳标志， 0x1C
    vl_uint8 needRsp:4;       // 0x0 表示不需要回复，0x1表示需要服务器回传外网ip：port
    vl_uint8 ver:4;           // 版本号
    vl_uint16 reserb:16;      // 保留
#endif
    vl_uint32 uid;            // 用户id
    vl_uint32 exp;            // 有效时长
#if defined(VL_BIG_ENDIAN)  // 大端
    vl_uint32 endTag:8;        // 结束标志，0x1C
    vl_uint32 reserb2:24;
#else  // 小端模式
    vl_uint32 reserb2:24;
  vl_uint32 endTag:8;        // 结束标志，0x1C
#endif
}RTPHBReq;

class SessionHeartbeat : public EptHeartBeat 
{//会话心跳
public:
    SessionHeartbeat(const char* ip, const vl_uint16 port, vl_uint32 uid, vl_uint32 expired,  vl_uint32 interval,vl_uint32 timeout)
    : EptHeartBeat(ip, port, interval, timeout), uid(uid)
 {
        RTPHBReq req = {0};
        req.magic = 0x1C;
        req.ver = 1;
        req.needRsp = (timeout > 0) ? 1 : 0;
        req.uid = htonl(uid);
        req.exp = htonl(expired);
        req.endTag = 0x1C;
        setHeartBeat(&req, sizeof(RTPHBReq));
    }

    SessionHeartbeat(const vl_uint32 ip, const vl_uint16 port, vl_uint32 uid, vl_uint32 expired,  vl_uint32 interval,vl_uint32 timeout)
    : EptHeartBeat(ip, port, interval, timeout), uid(uid)
 {
        RTPHBReq req = {0};
        req.magic = 0x1C;
        req.ver = 1;
        req.needRsp = (timeout > 0) ? 1 : 0;
        req.uid = htonl(uid);
        req.exp = htonl(expired);
        req.endTag = 0x1C;
        setHeartBeat(&req, sizeof(RTPHBReq));
    }

    vl_bool isRsp(void* data, vl_size length)
  {
        return VL_FALSE;
    }

private:
    vl_uint32 uid;

};

#endif















