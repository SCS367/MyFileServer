#include "AsyncLogging.h"
#include "Timestamp.h"

#include <stdio.h>

AsyncLogging::AsyncLogging(const std::string& basename,
                           off_t rollSize,
                           int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      basename_(basename),
      rollSize_(rollSize),
      thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
      mutex_(),
      cond_(),
      currentBuffer_(new Buffer),
      nextBuffer_(new Buffer),
      buffers_()
{
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::append(const char* logline, int len)
{
    // lock在构造函数中自动绑定它的互斥体并加锁，在析构函数中解锁，大大减少了死锁的风险
    std::lock_guard<std::mutex> lock(mutex_);
    // 当前缓冲（currentBuffer_）剩余空间（avail()）足够存放新log消息大小（len）时，就直接存放到当前缓冲；
    if (currentBuffer_->avail() > len)
    {
        currentBuffer_->append(logline, len);
    }
    else
    {
        /*
        缓冲剩余空间不够时，说明当前缓冲已满（或者接近已满），就将当前缓冲move到已满缓冲队列（buffers_），
        将空闲缓冲move到当前缓冲，再把新log消息存放到当前缓冲中（此时当前缓冲为空，剩余空间肯定够用），
        最后唤醒等待中的后端线程。
        */
       //Large Buffer是通过std::unique_ptr指向的，move操作后，原来的 std::unique_ptr就会值为空。
        buffers_.push_back(std::move(currentBuffer_));
        if (nextBuffer_) 
        {
            currentBuffer_ = std::move(nextBuffer_);
        } 
        else 
        {
            // 备用缓冲区也不够时，重新分配缓冲区，这种情况很少见
            currentBuffer_.reset(new Buffer);
        }
        currentBuffer_->append(logline, len);
        // 唤醒写入磁盘得后端线程
        //因为一个应用程序通常只有一个日志库后端，而一个后端通常只有一个后端线程，也只会有一个后端线程在该条件变量上等待，因此唤醒一个线程足以。
        cond_.notify_one();
    }
}

void AsyncLogging::threadFunc()
{
    // output有写入磁盘的接口
    LogFile output(basename_, rollSize_, false);
    //构建1个LogFile对象，用于控制log文件创建、写日志数据

    //创建2个空闲缓冲区buffer1、buffer2，和一个待写缓冲队列buffersToWrite，分别用于替换当前缓冲currentBuffer_、
    //空闲缓冲nextBuffer_、已满缓冲队列buffers_，避免在写文件过程中，锁住缓冲和队列，导致前端无法写数据到后端缓冲。
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    // 缓冲区数组置为16个，用于和前端缓冲区数组进行交换
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    //如果n大于vector当前的容量，reserve会对vector进行扩容，且当push_back的元素数量大于n的时候，
    //会重新分配一个大小为2n的新空间，再将原有的n的元素放入新开辟的内存空间中。其他情况下都不会重新分配vector的存储空间。
    while (running_)
    {
        {
            // 互斥锁保护，这样别的线程（写日志的主线程们）在这段时间就无法向前端Buffer数组写入数据
            /*
            每次当已满缓冲队列中有数据时，或者即使没有数据但3秒超时，就将当前缓冲加入到已满缓冲队列（即使当前缓冲没满），
            将buffer1移动给当前缓冲，buffer2移动给空闲缓冲（如果空闲缓冲已移动的话）。
            */
            std::unique_lock<std::mutex> lock(mutex_);
            if (buffers_.empty())
            {
                // 等待三秒也会解除阻塞
                cond_.wait_for(lock, std::chrono::seconds(3));
            }

            // 无论cond是因何（一是超时，二是当前缓冲区写满了）而醒来，都要将currentBuffer_放到buffers_中。  
      // 如果是因为时间到（3秒）而醒，那么currentBuffer_还没满，此时也要将之写入LogFile中。  
      // 如果已经有一个前端buffer满了，那么在前端线程中就已经把一个前端buffer放到buffers_中  
      // 了。此时，还是需要把currentBuffer_放到buffers_中（注意，前后放置是不同的buffer，  
      // 因为在前端线程中，currentBuffer_已经被换成nextBuffer_指向的buffer了）。
            buffers_.push_back(std::move(currentBuffer_)); //unique_ptr无法直接copy,最好用右值引用
            //首先unique_ptr只支持右值的拷贝构造(移动构造)和operator=，不支持复制操作。因此无论是哪一种做法， 本质上用户都只能够传右值。
            // 归还正使用缓冲区
            currentBuffer_ = std::move(newBuffer1);
            // 后端缓冲区和前端缓冲区交换
            //然后，再交换已满缓冲队列和待写缓冲队列，这样已满缓冲队列就为空，待写缓冲队列就有数据了。
            buffersToWrite.swap(buffers_);
            if (!nextBuffer_)
            {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        // 如果将要写入文件的buffer列表中buffer的个数大于25，那么将多余数据删除。
    // 前端陷入死循环，拼命发送日志消息，超过后端的处理能力，这是典型的生产速度超过消费速度，
    // 会造成数据在内存中的堆积，严重时引发性能问题(可用内存不足)或程序崩溃(分配内存失败)。


        //接着，将待写缓冲队列的所有缓冲通过LogFile对象，写入log文件。
        // 遍历所有 buffer，将其写入文件
        for (const auto& buffer : buffersToWrite)
        {
            output.append(buffer->data(), buffer->length());
        }

        //此时，待写缓冲队列中的缓冲，已经全部写到LogFile指定的文件中（也可能在内核缓冲中），
        //擦除多余缓冲，只用保留两个，归还给buffer1和buffer2。
        // 只保留两个缓冲区
        if (buffersToWrite.size() > 2)
        {
            buffersToWrite.resize(2);
        }

        // 归还newBuffer1缓冲区
        if (!newBuffer1)
        {
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        // 归还newBuffer2缓冲区
        if (!newBuffer2)
        {
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear(); // 清空后端缓冲区队列
        output.flush(); //清空文件缓冲区
    }
    output.flush();
}