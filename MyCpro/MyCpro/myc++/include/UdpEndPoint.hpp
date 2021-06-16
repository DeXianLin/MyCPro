#ifndef __UDP_ENDPOINT_HPP__
#define __UDP_ENDPOINT_HPP__

#include <pthread.h>
#include "EndPoint.hpp"
extern void incoming_trigger(int type, int send_id, int recv_id, int ssrc);
void* udp_ept_send_looper(void* userData);
void* udp_ept_recv_looper(void*userData );

class UdpEndPoint : public EndPoint, IOQObserver {
  friend void* udp_ept_send_looper(void* userData);
  friend void* udp_ept_recv_looper(void* userData);
public:
  UdpEndPoint(vl_uint16 port, MemoryPool* memPool, vl_size cacheSize) : 
    EndPoint(port, ESOCK_DGRAM, memPool, cacheSize), sending(VL_FALSE), recving(VL_FALSE), recv_thread(), send_thread(),
	rqLock(PTHREAD_MUTEX_INITIALIZER), sqLock(PTHREAD_MUTEX_INITIALIZER), 
	sqCond(PTHREAD_COND_INITIALIZER) {};

  virtual ~UdpEndPoint()
  {
  	stopSend();
  	stopRecv();
	pthread_mutex_destroy(&rqLock);
	pthread_mutex_destroy(&sqLock);
	pthread_cond_destroy(&sqCond);

  }

  /* 创建发送和接收线程 */
  virtual vl_status initial();
  /* 对外发送数据接口 */
  virtual vl_status sendTo(vl_uint32 remoteIp, vl_uint16 remotePort, void* data, vl_size length);
  virtual vl_status sendTo(Transmittable* data);
  virtual vl_status send(void* data, vl_size length);

  /* 接收开关，开始接收后，将自动接受到的数据经过EptProtocol流入ioq */
  virtual vl_status stopRecv();
  virtual vl_status stopSend();
  /* 开始停止 */
  virtual vl_status stop();

  /* 应用层编解码器管理接口 */
  virtual vl_status addProFilter(EptProtocol* pto);

  void onIOQueueUpdated();
protected:
  /* ioq接入接口 */
  virtual vl_status registerSendQueue(IOQueue* ioq);
  virtual vl_status registerRecvQueue(IOQueue* ioq);
  virtual vl_status unregisterSendQueue(IOQueue* ioq);
  virtual vl_status unregisterRecvQueue(IOQueue* io);

  /* 接收线程开关 */
  vl_bool recving;
  /* 接收线程 */
  pthread_t recv_thread;

  /* 发送线程开关 */
  vl_bool sending;
  /* 发送线程 */
  pthread_t send_thread;

  /* 发送队列锁 */
  pthread_mutex_t sqLock;
  pthread_cond_t sqCond;
  /* 接收队列锁 */
  pthread_mutex_t rqLock;
private:
  vl_status sendFromIOQ(IOQueue* ioq, vl_size *);
  vl_status recvToIOQ(UdpEndPoint * endpoint, void* data, vl_size length, sockaddr_in * addr_in);
};

#endif
