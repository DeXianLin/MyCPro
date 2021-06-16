#include <memory.h>
#include <signal.h>
#include <time.h>
#include "TimerTrigger.hpp"
//#include <Windows.h>
//#include <WinSock2.h>


#include <sys/socket.h>
#include <netinet/in.h>



TimerTrigger* TimerTrigger::instance20ms = NULL;
TimerTrigger* TimerTrigger::instanceSec = NULL;

static void timer_trigger_callback(void* data) {
    TimerTrigger* trigger = (TimerTrigger*)data;
    trigger->traval();
}

TimerTrigger::TimerTrigger(ETIMER_TRIGGER_INTERV intervalLevel) : lock(PTHREAD_MUTEX_INITIALIZER) {

    switch(intervalLevel) {
    case TIMER_TRIGGER_INTERV_20MS:
        msecInterval = 20;
        break;
    case TIMER_TRIGGER_INTERV_1S:
        msecInterval = 1000;
        break;
    default:
        msecInterval = 20;
        break;
    }
    timerReady = VL_TRUE;
}


TimerTrigger::~TimerTrigger()
{
    list<TickCounter*>::iterator it = waitList.begin();

    while(it != waitList.end()) {
        it = waitList.erase(it);
    }
    pthread_mutex_destroy(&lock);

    if(VL_TRUE == this->timerReady) {
        //zjw add 2021 1 27
        killTimer(this->timerID);
    }
}

TimerTrigger* TimerTrigger::getInstance20MS() {
    if(NULL == instance20ms) {
        instance20ms = new TimerTrigger(TIMER_TRIGGER_INTERV_20MS);
    }
    return instance20ms;
}

TimerTrigger* TimerTrigger::getInstanceSec() {
    if(NULL == instanceSec) {
        instanceSec = new TimerTrigger(TIMER_TRIGGER_INTERV_1S);
    }
    return instanceSec;
}


void TimerTrigger::resetInterval(vl_uint32 msec) {
    this->msecInterval = msec;
}

vl_bool TimerTrigger::registerWaiter(TickCounter *waiter)
{
    if(VL_TRUE != timerReady) {
        printf("Timer trigger register failed, timer not ready");
        return VL_FALSE;
    }

    if(waitList.size() == 0) {
        printf("Timer trigger start timer interval = %d ms", msecInterval);

    }

    this->timerID = startTimer(10*1000);
    /* 若计数对象已存在，复位计数 */
    list<TickCounter*>::iterator it = waitList.begin();
    while(it != waitList.end()) {
        if((*it) == waiter) {
            waiter->count = 0;
            break;
        }
        it++;
    }
    /* 计数对象不存在，添加到计数列表 */
    if(it == waitList.end()) {
        pthread_mutex_lock(&this->lock);
        waitList.push_back(waiter);
        pthread_mutex_unlock(&this->lock);
    }
    return VL_TRUE;
}

void TimerTrigger::unregisterWaiter(TickCounter* waiter) {
    if(waitList.size() > 0) {
        pthread_mutex_lock(&this->lock);
        list<TickCounter*>::iterator it = waitList.begin();
        while(it != waitList.end()) {
            if(waiter == *it) {
                waitList.erase(it);
                break;
            }
            it++;
        }
        /* 当前没有等待触发队列，停止定时器 */
        if(waitList.size() == 0) {
            cancelTimer();
        }
        pthread_mutex_unlock(&this->lock);
    }
}

void TimerTrigger::cancelTimer()
{
    printf("Timer trigger, remove waiter and cancel timer");
    killTimer(this->timerID);
}

vl_uint32 TimerTrigger::calculate(vl_uint32 msec) const {
    vl_uint32 count = msec / this->msecInterval;
    if(0 != (msec % this->msecInterval)) {
        count ++;
    }
    return count;
}


void TimerTrigger::traval()
{
    list<TickCounter*>::iterator it = waitList.begin();
    list<TickCounter*> lstreStart;
    //printf("timer_trigger_callback");
    pthread_mutex_lock(&this->lock);
    while(it != waitList.end()) {
        TickCounter* counter = (*it);
        ETIMER_TRIGGER_RET ret = counter->matchCond(counter->count);

        switch (ret)
        {
        case TIMER_TRIGGER_RELASE:
            it = waitList.erase(it);
            /* 自动释放，判断队列是否为空 */
            if(waitList.size() == 0) {
                cancelTimer();
                printf("TimerTrigger no waiters ... ");
            }
            break;
        case TIMER_TRIGGER_RESTART:
            it = waitList.erase(it);
            lstreStart.push_back(counter);
            if(waitList.size() == 0) {
                cancelTimer();
                printf("TimerTrigger no waiters ... ");
            }
            counter->count = 0;
            break;
        case TIMER_TRIGGER_RESET:
            counter->count = 0;
            it++;
            break;
        case TIMER_TRIGGER_INC:
            counter->count++;
            it++;
            break;
        default:
            printf("timer trigger counter return unknown value");
            it++;
            break;
        }
    }
    pthread_mutex_unlock(&this->lock);
    it = lstreStart.begin();
    while(it != lstreStart.end())
    {
        this->registerWaiter(*it++);
    }
}


void TimerTrigger::timerEvent(QTimerEvent *event)
{
    timer_trigger_callback(this);
}

