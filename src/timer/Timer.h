#ifndef TIMER_H
#define TIMER_H

#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>

/**
 * Timer用于描述一个定时器
 * 定时器回调函数，下一次超时时刻，重复定时器的时间间隔等
 */
class Timer : noncopyable
{
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0) // 一次性定时器设置为0
    {
    }

    // 调用此定时器的回调函数
    void run() const 
    { 
        callback_(); 
    }

    // 返回此定时器超时时间
    Timestamp expiration() const  { return expiration_; }

    bool repeat() const { return repeat_; }

    // 重启定时器(如果是非重复事件则到期时间置为0)
    void restart(Timestamp now);
        /*
    观察定时器构造函数的repeat_(interval > 0.0)，这里会根据「间隔时间是否大于0.0」来判断此定时器是重复使用的还是一次性的。
     如果是重复使用的定时器，在触发任务之后还需重新利用。这里就会调用 restart 方法。
     我们设置其下一次超时时间为「当前时间 + 间隔时间」。如果是「一次性定时器」，那么就会自动生成一个空的 Timestamp，
     其时间自动设置为 0.0。
    */

private:
    const TimerCallback callback_;  // 定时器回调函数
    Timestamp expiration_;          // 下一次的超时时刻
    const double interval_;         // 超时时间间隔(超时后设置下一次超时时间)，如果是一次性定时器，该值为0
    const bool repeat_;             // 是否重复(false 表示是一次性定时器)
};

#endif // TIMER_H