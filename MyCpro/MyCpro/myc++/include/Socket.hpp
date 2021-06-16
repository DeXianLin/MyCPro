#ifndef __SOCK_ADDR_HPP__
#define __SOCK_ADDR_HPP__

#include <stdlib.h>
//#include <WinSock2.h>
#include "vl_types.h"
#include "vl_const.h"
//#include <WS2tcpip.h>

#include <sys/socket.h>
#include<netinet/in.h>

typedef enum ESOCK_TYPE {
    ESOCK_STREAM = SOCK_STREAM,
    ESOCK_DGRAM = SOCK_DGRAM,
    ESOCK_RAW = SOCK_RAW,
    ESOCK_RDM = SOCK_RDM,
    ESOCK_SEQPACKET = SOCK_SEQPACKET
} ESOCK_TYPE;

/**
 * socket地址封装类，重载赋值和判断是否相等操作符
 */
class SockAddr {
public:
    SockAddr(const sockaddr_in* addr_in)
    {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = addr_in->sin_family;
        addr.sin_addr.s_addr = addr_in->sin_addr.s_addr;
        addr.sin_port = addr_in->sin_port;
    }
    SockAddr(const char* ip, vl_uint16 port)
    {
        memset(&addr,0, sizeof(addr));
        inet_pton(AF_INET, ip, &addr.sin_addr);
        addr.sin_port = htons(port);
    }
    SockAddr(vl_uint16 port)
    {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
    }
    SockAddr(vl_uint32 ip, vl_uint16 port)
    {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(ip);
        addr.sin_port = htons(port);
    }
    SockAddr(const SockAddr& ori)
    {
        memcpy(&addr, &ori.addr, sizeof(addr));
    }
    in_addr ip_sin()
    {
        return addr.sin_addr;
    }
    vl_uint32 ip()
    {
        return ntohl(addr.sin_addr.s_addr);
    }
    vl_uint16 port()
    {
        return ntohs(addr.sin_port);
    }
    bool operator==(const SockAddr& rAddr) const
    {
        if (
            this->addr.sin_addr.s_addr == rAddr.addr.sin_addr.s_addr
            && this->addr.sin_port == rAddr.addr.sin_port) {
            return true;
        }
        return false;
    }
    SockAddr& operator=(SockAddr& rAddr)
    {
        addr.sin_family = rAddr.addr.sin_family;
        addr.sin_addr.s_addr = rAddr.addr.sin_addr.s_addr;
        addr.sin_port = rAddr.addr.sin_port;
        return *this;
    }
    sockaddr_in* getNativeAddr()
    {
        return &(this->addr);
    }
private:
    sockaddr_in addr;
};

/**
 * tcp / udp socket 抽象工具类
 */
class Socket {
public:
    Socket(ESOCK_TYPE sockType) :sockfd(-1), sockType(sockType), named(VL_FALSE)
    {
        sockfd = ::socket(AF_INET, sockType, 0);
        if (sockfd == -1) {
            printf("Socket create socket failed");
        }
    }
    virtual ~Socket()
    {
        if (sockfd != -1)
        {
            closesocket(sockfd);
            sockfd = -1;
        }
    }
    int getFd()
    {
        return sockfd;
    }
    int getSockType()
    {
        return sockType;
    }
    vl_status bind(SockAddr* addr)
    {
        if (sockfd < 0)
        {
            return VL_ERR_SOCK_NOT_OPEND;
        }
        int err = ::bind(sockfd, (const struct sockaddr*) addr->getNativeAddr(),
                         sizeof(struct sockaddr_in));
        if (-1 != err)
        {
            named = VL_TRUE;
            return VL_SUCCESS;
        } else
        {
            named = VL_FALSE;
            return VL_ERR_SOCK_BIND;
        }
    }
    vl_bool isNamed() {
        return named;
    }
    vl_bool isOpend() {
        return sockfd >= 0;
    }
    vl_status setReuse(vl_bool reuse)
    {
        if (sockfd >= 0)
        {
            int err = 1;
            char opt = 1;
            if (VL_FALSE == reuse)
            {
                opt = 0;
            }
            err = ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt,
                               sizeof(opt));
            if (-1 == err)
            {
                return VL_ERR_SOCK_SETOPT;
            } else {
                return VL_SUCCESS;
            }
        } else {
            return VL_ERR_SOCK_NOT_OPEND;
        }
    }
    vl_status getReuse(vl_bool* reuse)
    {
        if (sockfd >= 0) {
            int err;
            char opt;
            socklen_t outs = sizeof(opt);
            err = ::getsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, &outs);

            if (-1 != err) {
                if (outs == 0) {
                    *reuse = VL_FALSE;
                } else {
                    *reuse = VL_TRUE;
                }
                return VL_SUCCESS;
            } else {
                printf("Socket get reuse error");
                return VL_ERR_SOCK_GETOPT;
            }
        } else {
            printf("Socket get reuse, sock not opend");
            return VL_ERR_SOCK_NOT_OPEND;
        }
    }
    vl_status getSendBufferSize(vl_size* size)
    {
        if (sockfd >= 0)
        {
            int err;
            char opt;
            socklen_t outs = sizeof(opt);
            err = ::getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &opt, &outs);
            if (-1 != err) {
                *size = opt;
                return VL_SUCCESS;
            } else {
                printf("Sockt get send buffer size error");
                return VL_ERR_SOCK_GETOPT;
            }
        } else {
            printf("Socket get send buffer size failed, sock not opend");
            return VL_ERR_SOCK_NOT_OPEND;
        }
    }
    vl_status setSendBufferSize(vl_size bufsize)
    {
        if (sockfd >= 0) {
            int err = bufsize;
            int opt = bufsize;
            err = ::setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char*)&opt,sizeof(opt));
            if (-1 != err) {
                return VL_SUCCESS;
            } else {
                printf("Sockt set send buffer size error");
                return VL_ERR_SOCK_SETOPT;
            }
        } else {
            printf("Socket set send buffer size failed, sock not opend");
            return VL_ERR_SOCK_NOT_OPEND;
        }
    }
    vl_status getRecvBufferSize(vl_size* size)
    {
        if (sockfd >= 0) {
            int err;
            int opt;
            socklen_t outs = sizeof(opt);
            err = ::getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&opt, &outs);
            if (-1 != err) {
                *size = opt;
                return VL_SUCCESS;
            } else {
                printf("Sockt get send buffer size error");
                return VL_ERR_SOCK_GETOPT;
            }
        } else {
            printf("Socket get send buffer size failed, sock not opend");
            return VL_ERR_SOCK_NOT_OPEND;
        }
    }
    vl_status setRecvBufferSize(vl_size bufsize)
    {
        if (sockfd >= 0) {
            int err = bufsize;
            int opt = bufsize;
            err = ::setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&opt,sizeof(opt));
            if (-1 != err) {
                return VL_SUCCESS;
            } else {
                printf("Sockt set send buffer size error");
                return VL_ERR_SOCK_SETOPT;
            }
        } else {
            printf("Socket set recv buffer size failed, sock not opend");
            return VL_ERR_SOCK_NOT_OPEND;
        }
    }
protected:
    int sockfd;
    /* socket类型 */
    ESOCK_TYPE sockType;
    /* socket是否已命名 */
    vl_bool named;
};

#endif

