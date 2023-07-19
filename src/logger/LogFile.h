#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "FileUtil.h"

#include <mutex>
#include <memory>


//LogFile 主要职责：提供对日志文件的操作，包括滚动日志文件、将log数据写到当前log文件、flush log数据到当前log文件。
class LogFile
{
public:
    LogFile(const std::string& basename,
            off_t rollSize,
            int flushInterval = 3,
            int checkEveryN = 1024);
    ~LogFile();

    void append(const char* data, int len);
    void flush();
    bool rollFile(); // 滚动日志

private:
    static std::string getLogFileName(const std::string& basename, time_t* now);
    void appendInLock(const char* data, int len);

    const std::string basename_;  // 基础文件名, 用于新log文件命名
    const off_t rollSize_;  // 滚动文件大小
    const int flushInterval_; // 冲刷时间限值, 默认3 (秒)
    const int checkEveryN_;  // 写数据次数限值, 默认1024

    int count_;  // 写数据次数计数, 超过限值checkEveryN_时清除, 然后重新计数

    std::unique_ptr<std::mutex> mutex_;  // 互斥锁指针, 根据是否需要线程安全来初始化
    time_t startOfPeriod_;  // 本次写log周期的起始时间(秒)
    time_t lastRoll_;  // 上次roll日志文件时间(秒)
    time_t lastFlush_;  // 上次flush日志文件时间(秒)
    std::unique_ptr<FileUtil> file_;

    const static int kRollPerSeconds_ = 60*60*24;
};

#endif // LOG_FILE_H