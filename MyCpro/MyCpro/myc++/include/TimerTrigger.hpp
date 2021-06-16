#ifndef __TIMER_TRIGGER_HPP__
#define __TIMER_TRIGGER_HPP__

#include <time.h>
#include <list>
#include <pthread.h>
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif
#include "sys/timeb.h"
#include "vl_types.h"


using namespace std;
class TimerTrigger;
extern QTimer *timer;
/**
 * 注意：定时计数器，只负责处理计数逻辑，回调之应该做一些状态改变的事情，逻辑操作和耗时操作，应另起线程处理
 */

typedef enum {
    TIMER_TRIGGER_RESET,
    TIMER_TRIGGER_INC,
    TIMER_TRIGGER_RELASE,
    TIMER_TRIGGER_RESTART
}ETIMER_TRIGGER_RET;

typedef enum {
    TIMER_TRIGGER_INTERV_20MS,
    TIMER_TRIGGER_INTERV_1S
}ETIMER_TRIGGER_INTERV;

/**
 * 外部计数器，由TimerTigger负责管理计数，外部逻辑负责条件判断，是否重置时间。
 */
class TickCounter {
public:
    friend class TimerTrigger;
    TickCounter(vl_bool autoUnreg = VL_TRUE) : count(0), autoUnreg(autoUnreg) {}
    /* 判断是否符合触发条件，符合触发条件时，计数器将重置 */
    virtual ETIMER_TRIGGER_RET matchCond(vl_uint32 count) = 0;
protected:
    vl_uint32 count;
    vl_bool autoUnreg;
};

/**
 * 本地timer全局实现，当有注册回调时，以指定时间间隔触发调用所有的入口接口
 */
class TimerTrigger :public QObject{
public:
    static TimerTrigger* getInstance20MS();
    static TimerTrigger* getInstanceSec();
    /* 设置定时器触发间隔 */
    void resetInterval(vl_uint32 msec);
    /* 注册定时器等待事件，有时间时保持定时器触发 */
    vl_bool registerWaiter(TickCounter* waiter);
    /* 注销定时器等待时间，队列为空时，停止定时器 */
    void unregisterWaiter(TickCounter* waiter);
    /* 遍历等待列表 */
    void traval();
    /* 计算一个时间段在该计数器所需要的数量 */
    vl_uint32 calculate(vl_uint32 msec) const ;
    vl_uint32 getIntervalMS() const { return msecInterval; }
    /*zjw 2021127 add*/
    virtual void timerEvent(QTimerEvent *event) override;
    ~TimerTrigger();
private:
    static TimerTrigger* instance20ms;
    static TimerTrigger* instanceSec;

    TimerTrigger(ETIMER_TRIGGER_INTERV intervalLevel);
    /* 内部暂停定时器接口 */
    void cancelTimer();
    pthread_mutex_t lock;
    list<TickCounter*> waitList;
    vl_uint32 msecInterval;
    int timerID; //zjw add 2021127
    vl_bool timerReady;
};

#endif

