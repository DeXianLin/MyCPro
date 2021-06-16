#ifndef __EPT_HEARTBEAT_HPP__
#define __EPT_HEARTBEAT_HPP__

#include <stdlib.h>
#include "vl_types.h"
#include "vl_const.h"
#include "vl_time.hpp"
//#include <WinSock2.h>

#include <sys/socket.h>
#include <netinet/in.h>


#define __MALLOC malloc
#define __FREE free
#define __MEMCPY memcpy

extern "C" {
//void notifyMediaHeartbeatReceive(int i);
}
/**
 * 心跳基类。
 * 心跳时间以秒为单位，心跳协议与普通的业务协议区分开，优先判断是否为网络心跳协议。
 */
class EptHeartBeat 
{

public:
    virtual ~EptHeartBeat()
    {
        if (NULL != this->data)
        {
            __FREE(this->data);
        }
    }

    EptHeartBeat(const char* ip, const vl_uint16 port, vl_uint32 interval, vl_uint32 timeout,
                 void* data = NULL, vl_size length = 0)
        : remotePort(port), interval(interval), timeoutSec(timeout), data(NULL), length(0)
    {
        this->beatTimes = 0;
        this->rspTimes = 0;

        long tempIp = inet_addr(ip);
        remoteIp = ntohl(tempIp);

        if (NULL != data && length != 0) {
            this->setHeartBeat(data, length);
        }
    }

    /**
   * 初始化远程ip，端口
   */
    EptHeartBeat(const vl_uint32 ip, const vl_uint16 port, vl_uint32 interval,vl_uint32 timeout,
                 void* data = NULL, vl_size length = 0)
        : remoteIp(ip), remotePort(port), interval(interval), timeoutSec(timeout), data(NULL), length(0)
    {
        this->beatTimes = 0;
        this->rspTimes = 0;
        if (NULL != data && length != 0)
        {
            this->setHeartBeat(data, length);
        }
    }

    EptHeartBeat(const EptHeartBeat& rhs) : data(NULL),length(0)
    {
        this->beatTimes = 0;
        this->rspTimes = 0;
        vl_uint32 ip = rhs.getRemoteIp();
        printf("EptHeartBeat copy construct, remote ip = %d.%d.%d.%d remote port = %d ", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
               (ip >> 8) & 0xFF,(ip) & 0xFF, rhs.getRemotePort());

        deepCopy(rhs);
    }

    EptHeartBeat& operator=(const EptHeartBeat& rhs)
    {
        printf("EptHeartBeat copy from equal override !!!");
        deepCopy(rhs);
        return *this;
    }

    /**
   *  产生心跳数据
   */
    virtual vl_status getHearBeat(void ** data, vl_size * len) const
    {
        if (NULL != data && length != 0)
        {
            *data = this->data;
            *len = this->length;
            return VL_SUCCESS;
        }
        printf("EndPoint get heartbeat failed");
        return VL_FAILED;
    }
    /**
   * 设置心跳，心跳内容采用内存拷贝方式
   */
    vl_status setHeartBeat(void* data, vl_size length)
    {
        if (NULL != this->data)
        {
            __FREE(this->data);

        }
        this->data = __MALLOC(length);
        __MEMCPY(this->data, data, length);
        this->length = length;
        return VL_SUCCESS;
    }
    /**
   * 是否需要心跳回复
   */
    vl_bool needRsp() const
    {
        if(0 == timeoutSec)
        {
            return VL_FALSE;

        } else
        {
            return VL_TRUE;
        }
    }

    vl_uint32 getRemoteIp() const { return remoteIp; }
    vl_uint16 getRemotePort() const { return remotePort; }
    vl_uint32 getInterval() const { return interval; }
    vl_uint32 getTimeoutSec() const {return timeoutSec; }

    // for logs
    void markHeartBeat() {
        ++this->beatTimes;

        time_t _time;
        char buffer[40];
        struct tm* tm_info;
        time(&_time);
        tm_info = localtime(&_time);

        strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", tm_info);

        printf("%s EptHeartBeat send remote %d.%d.%d.%d:%d",buffer,
               (remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF, (remoteIp >> 8) & 0xFF,(remoteIp) & 0xFF, remotePort);
    }

    void onResponse() {
        ++this->rspTimes;
        //notifyMediaHeartbeatReceive(remoteIp);
    }

    vl_bool isResponse(void *packet, vl_size pktlen)
    {
        if(20 == pktlen &&  0x1F == *((char*)packet)) {
            printf("EptHeartBeat response !!!");
            return VL_TRUE;
        }
        return VL_FALSE;
    }

    void resetBeatLog() {
        this->rspTimes = 0;
        this->beatTimes = 0;
    }

    void triggerLog() {
        printf("EptHeartBeat remote %d.%d.%d.%d:%d sent=%d, rsp=%d",
               (remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF, (remoteIp >> 8) & 0xFF,(remoteIp) & 0xFF, remotePort,
               beatTimes, rspTimes);
    }
private:
    void deepCopy(const EptHeartBeat& rhs) {
        this->remoteIp = rhs.remoteIp;
        this->remotePort = rhs.remotePort;
        if(this->data != NULL && rhs.data != this->data) {
            __FREE(this->data);
            this->data = NULL;
            this->length = 0;
        }

        if(rhs.length > 0 && rhs.data != NULL) {
            this->data = __MALLOC(rhs.length);
            this->length = rhs.length;

            __MEMCPY(this->data, rhs.data, rhs.length);
        }

        this->maxRetrans = rhs.maxRetrans;
        this->retransTimes = rhs.retransTimes;
        this->interval = rhs.interval;
        this->timeoutSec = rhs.timeoutSec;
    }

    /* 远程ip地址 */
    vl_uint32 remoteIp;
    vl_uint16 remotePort;
    /* 心跳数据 */
    void * data;
    /* 心跳数据长度 */
    vl_size length;
    /* 最大重发次数 */
    vl_uint32 maxRetrans;
    /* 当前重发次数 */
    vl_uint32 retransTimes;
    /* 心跳间隔 */
    vl_uint32 interval;
    /* 心跳超时时间 */
    vl_uint32 timeoutSec;

    // for debug
    vl_size beatTimes;
    vl_size rspTimes;
};

#endif

