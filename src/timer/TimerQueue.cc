#include "EventLoop.h"
#include "Channel.h"
#include "Logging.h"
#include "Timer.h"
#include "TimerQueue.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>

int createTimerfd()
{
    /**
     * CLOCK_MONOTONIC：绝对时间
     * TFD_NONBLOCK：非阻塞
     */
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                    TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_ERROR << "Failed in timerfd_create";
    }
    return timerfd;
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop_, timerfd_),
      timers_()
{
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this)); //定时器读触发事件(超时)
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{   
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    // 删除所有定时器
    for (const Entry& timer : timers_)
    {
        delete timer.second; //非智能指针
    }
}

void TimerQueue::addTimer(TimerCallback cb,
                          Timestamp when,
                          double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    /*
    EventLoop调用方法，加入一个定时器事件，会向里传入定时器回调函数，超时时间和间隔时间（为0.0则为一次性定时器），
    addTimer方法根据这些属性构造新的定时器。
    定时器队列内部插入此定时器，并判断这个定时器的超时时间是否比先前的都早。如果是最早触发的，
    就会调用resetTimerfd方法重新设置tiemrfd_的触发时间。内部会根据超时时间和现在时间计算出新的超时时间。
    */
    loop_->runInLoop(
        std::bind(&TimerQueue::addTimerInLoop, this, timer));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    // 是否取代了最早的定时触发时间
    bool eraliestChanged = insert(timer);

    // 我们需要重新设置timerfd_触发时间
    if (eraliestChanged)
    {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

// 重置timerfd
void TimerQueue::resetTimerfd(int timerfd_, Timestamp expiration)
{   //timerfd_settime, 用于启动或停止绑定到fd的定时器。
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, '\0', sizeof(newValue));
    memset(&oldValue, '\0', sizeof(oldValue));

    // 超时时间 - 现在时间
    int64_t microSecondDif = expiration.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microSecondDif < 100)
    {
        microSecondDif = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
        microSecondDif / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microSecondDif % Timestamp::kMicroSecondsPerSecond) * 1000);
    newValue.it_value = ts;
    // 此函数会唤醒事件循环
    if (::timerfd_settime(timerfd_, 0, &newValue, &oldValue)) //我们使用此函数启动或停止定时器。new_value：设置超时事件，设置为0则表示停止定时器
    //old_value：原来的超时时间，不使用可以置为NULL
    {
        LOG_ERROR << "timerfd_settime faield()";
    }
}

void ReadTimerFd(int timerfd) 
{
    uint64_t read_byte;
    ssize_t readn = ::read(timerfd, &read_byte, sizeof(read_byte));
    
    if (readn != sizeof(read_byte)) {
        LOG_ERROR << "TimerQueue::ReadTimerFd read_size < 0";
    }
}

// 返回删除的定时器节点 （std::vector<Entry> expired）
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> expired;
    // TODO:???
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry); //返回第一个大于等于x的数（x的下界），如果没找到，返回末尾的迭代器位置
    std::copy(timers_.begin(), end, back_inserter(expired));
    //在原来的set上去掉这些过期的定时器
    timers_.erase(timers_.begin(), end);
    
    return expired;
}

void TimerQueue::handleRead()
{
    Timestamp now = Timestamp::now();
    ReadTimerFd(timerfd_); //只是读一下，存入一个int中就行

    std::vector<Entry> expired = getExpired(now);
    //返回删除的定时器节点

    // 遍历到期的定时器，调用回调函数
    callingExpiredTimers_ = true;
    for (const Entry& it : expired)
    {
        it.second->run(); //timer->cakkback()   
    }
    callingExpiredTimers_ = false;
    
    // 重新设置这些定时器
    reset(expired, now);

}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;
    for (const Entry& it : expired)
    {
        // 重复任务则继续执行
        if (it.second->repeat())
        {
            auto timer = it.second;
            timer->restart(Timestamp::now());
            insert(timer);
        }
        else
        {
            delete it.second;
        }

        // 如果重新插入了定时器，需要继续重置timerfd
        if (!timers_.empty())
        {
            resetTimerfd(timerfd_, (timers_.begin()->second)->expiration());
        }
    }
}

bool TimerQueue::insert(Timer* timer)
{
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first)
    {
        //内部实现的插入方法获取此定时器的超时时间，如果比先前的时间小就说明第一个触发。那么我们会设置好布尔变量。因此这涉及到timerfd_的触发时间。
        // 说明最早的定时器已经被替换了
        earliestChanged = true;
    }

    // 定时器管理红黑树插入此新定时器，set自动排序
    timers_.insert(Entry(when, timer));

    return earliestChanged;
}


