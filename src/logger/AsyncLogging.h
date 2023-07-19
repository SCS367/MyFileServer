#ifndef ASYNC_LOGGING_H
#define ASYNC_LOGGING_H

#include "noncopyable.h"
#include "Thread.h"
#include "FixedBuffer.h"
#include "LogStream.h"
#include "LogFile.h"


#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

/*
AsyncLogging 主要职责：提供大缓冲Large Buffer（默认4MB）存放多条日志消息，缓冲队列BufferVector用于存放多个Large Buffer，
为前端线程提供线程安全的写Large Buffer操作；提供专门的后端线程，用于定时或缓冲队列非空时，
将缓冲队列中的Large Buffer通过LogFile提供的日志文件操作接口，逐个写到磁盘上。
*/

class AsyncLogging
{
public:
    AsyncLogging(const std::string& basename,
                 off_t rollSize,
                 int flushInterval = 3);
    ~AsyncLogging()
    {
        if (running_)
        {
            stop();
        }
    }

    // 前端调用 append 写入日志
    void append(const char* logling, int len);

    void start()
    {
        running_ = true;
        thread_.start();
    }

    void stop()
    {
        running_ = false;
        cond_.notify_one();
        thread_.join();
    }

private:
    using Buffer = FixedBuffer<kLargeBuffer>;
    using BufferVector = std::vector<std::unique_ptr<Buffer>>;
    using BufferPtr = BufferVector::value_type; // 已满缓冲队列类型

    void threadFunc();

    const int flushInterval_;  // 冲刷缓冲数据到文件的超时时间, 默认3秒
    std::atomic<bool> running_; // 后端线程loop是否运行标志
    const std::string basename_;  // 日志文件基本名称
    const off_t rollSize_;  // 日志文件滚动大小
    Thread thread_;  // 后端线程
    std::mutex mutex_;
    std::condition_variable cond_;

    BufferPtr currentBuffer_;  // 当前缓冲
    BufferPtr nextBuffer_;  // 空闲缓冲
    BufferVector buffers_;  // 已满缓冲队列
};

#endif // ASYNC_LOGGING_H