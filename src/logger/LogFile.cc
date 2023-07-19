#include "LogFile.h"

LogFile::LogFile(const std::string& basename,
        off_t rollSize,
        int flushInterval,
        int checkEveryN)
    : basename_(basename),
      rollSize_(rollSize),
      flushInterval_(flushInterval),
      checkEveryN_(checkEveryN),
      count_(0),
      mutex_(new std::mutex),
      startOfPeriod_(0),
      lastRoll_(0),
      lastFlush_(0)
{
    //重新启动时，可能并没有log文件，因此在构建LogFile对象时，直接调用rollFile()以创建一个全新的日志文件。
    rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char* data, int len)
{
    std::lock_guard<std::mutex> lock(*mutex_);
    appendInLock(data, len);
}

/*
append_unlocked 会先将log消息写入file_文件，之后再判断是否需要滚动日志文件；如果不滚动，
就根据append_unlocked的调用次数和时间，确保1）一个log文件超时（默认1天），就创建一个新的；
2）flush文件操作，不会频繁执行（默认间隔3秒）。
*/
void LogFile::appendInLock(const char* data, int len)
{
    file_->append(data, len);

    if (file_->writtenBytes() > rollSize_)
    {
        rollFile();
    }
    else
    {
        ++count_;
        if (count_ >= checkEveryN_)
        {
            count_ = 0;
            time_t now = ::time(NULL);
            time_t thisPeriod = now / kRollPerSeconds_ * kRollPerSeconds_;
            if (thisPeriod != startOfPeriod_)
            {
                rollFile();
            }
            else if (now - lastFlush_ > flushInterval_)
            {
                lastFlush_ = now;
                file_->flush();
            }
        }
    }
}


void LogFile::flush()
{
    // std::lock_guard<std::mutex> lock(*mutex_);
    file_->flush();
}

// 滚动日志
// basename + time + ".log"
bool LogFile::rollFile()
{
    time_t now = 0;
    std::string filename = getLogFileName(basename_, &now); //取得新log文件名，文件名全局唯一
    // 计算现在是第几天 now/kRollPerSeconds求出现在是第几天，再乘以秒数相当于是当前天数0点对应的秒数
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

    if (now > lastRoll_)
    {
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;
        // 让file_指向一个名为filename的文件，相当于新建了一个文件
        // fopen一个文件
        file_.reset(new FileUtil(filename)); //用RAII方式管理文件资源，构建对象即打开文件，销毁对象即关闭文件。
        return true;
    }
    return false;
}


//getLogFileName根据调用者提供的基础名，以及当前时间，得到一个全新的、唯一的log文件名。
std::string LogFile::getLogFileName(const std::string& basename, time_t* now)
{
    std::string filename;
    filename.reserve(basename.size() + 64);
    filename = basename;

    char timebuf[32];
    struct tm tm;
    *now = time(NULL);
    localtime_r(now, &tm);
    // 写入时间
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S", &tm);
    filename += timebuf;

    filename += ".log";

    return filename;
}