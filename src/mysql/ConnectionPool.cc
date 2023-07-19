#include "ConnectionPool.h"

#include <fstream>
#include <thread>
#include <assert.h>

ConnectionPool* ConnectionPool::getConnectionPool()
{
    static ConnectionPool pool;
    return &pool;
}

ConnectionPool::ConnectionPool()
{
    // assert();
    bool b = parseJsonFile();
    std::cout<<"json"<<b<<std::endl;
//然后我们需要创建一定数量的数据库连接，数据库连接池会维持一个最小连接数量，如果有必要会在后面继续创建数据库连接，
//但是不会超过维护的最大连接数。 这里就是调用我们之前封装好的接口，创建数据库连接，并记录该连接的时间戳。
    for (int i = 0; i < minSize_; ++i)
    {
        addConnection(); //向连接池队列里加MysqlConn*
        currentSize_++;
    }
    // 开启新线程执行任务
    std::thread producer(&ConnectionPool::produceConnection, this);
    std::thread recycler(&ConnectionPool::recycleConnection, this);
    // 设置线程分离，不阻塞在此处
    producer.detach();
    recycler.detach();
}

ConnectionPool::~ConnectionPool()
{
    // 释放队列里管理的MySQL连接资源
    while (!connectionQueue_.empty())
    {
        MysqlConn* conn = connectionQueue_.front();
        connectionQueue_.pop();
        delete conn;
        currentSize_--;
    }
}

// 解析JSON配置文件
//我们的连接池保存了需要连接的数据库的信息，比如登录用户名，用户密码等。我们需要将这些信息写到配置文件中，这里用 JSON 格式储存。
bool ConnectionPool::parseJsonFile()
{
    std::ifstream file("conf.json");
    json conf = json::parse(file);

    ip_ = conf["ip"];
    user_ = conf["userName"];
    passwd_ = conf["password"];
    dbName_ = conf["dbName"];
    port_ = conf["port"];
    minSize_ = conf["minSize"];
    maxSize_ = conf["maxSize"];
    timeout_ = conf["timeout"];
    maxIdleTime_ = conf["maxIdleTime"];
    return true;
}

void ConnectionPool::produceConnection()
{
    while (true)
    {
        // RALL手法封装的互斥锁，初始化即加锁，析构即解锁
        std::unique_lock<std::mutex> locker(mutex_);
        while (!connectionQueue_.empty())
        {
            // produceConnection() 当数据库连接的数量大于等于最小连接数的时候，我们是不需要创建新连接。
            //这个时候 producer 线程就会被阻塞。否则调用 addConnection() 创建新的数据库连接，并唤醒所有被阻塞的线程。
            cond_.wait(locker);
        } 
        
        // 还没达到连接最大限制
        if (currentSize_ < maxSize_)
        {
            addConnection();
            currentSize_++;
            // 唤醒被阻塞的线程
            cond_.notify_all();
        }       
    }
}

// 销毁多余的数据库连接
void ConnectionPool::recycleConnection()
{
    while (true)
    {
        // 周期性的做检测工作，每500毫秒（0.5s）执行一次
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        std::lock_guard<std::mutex> locker(mutex_);
        // 连接数位于(maxSize, maxSize]时候，可能会有空闲连接等待太久
        //recycleConnection() 在后台周期性的做检测工作，每 500 毫秒检测一次数据库连接池中所维持连接的数量，
        //如果超过了最大的连接数则要判断连接池队列里各个连接的存活时间，如果存活时间超过限制则销毁改连接。
        while (connectionQueue_.size() > minSize_)
        {
            MysqlConn* conn = connectionQueue_.front();
            if (conn->getAliveTime() >= maxIdleTime_)
            {
                // 存在时间超过设定值则销毁
                connectionQueue_.pop();
                delete conn;
                currentSize_--;
            }
            else
            {
                // 按照先进先出顺序，前面的没有超过后面的肯定也没有
                break;
            }
        }
    }
}

void ConnectionPool::addConnection()
{
    MysqlConn* conn = new MysqlConn;
    conn->connect(user_, passwd_, dbName_, ip_, port_);
    conn->refreshAliveTime();    // 刷新起始的空闲时间点
    connectionQueue_.push(conn); // 记录新连接
    currentSize_++;
}

// 获取连接
/*
我们的线程池对外的接口之一就是 getConnection 函数，我们通过此函数从数据库连接池中获取一个可用的数据库连接，
从而避免了重复创建新连接。 在获取连接的时候需要考虑连接池有没有可用的连接，当连接池可用连接为空时，会阻塞一段时间。
\这个时候就涉及到了之前的 produceConnection 函数了。如果可用连接不够用且维护连接数没到限制值，则会创建新连接。
创建成功后会唤醒在此处阻塞的线程们。 还有一件事情，我们要维护连接。因此，不仅要做到能给出连接，还要做到能回收连接。
我们该如何回收连接呢？这里我们使用的是智能指针的特性解决的，我们可以用一个智能指针管理连接资源，将此智能指针传出给外面的调用者。
此智能指针绑定了自定义的删除器，当其析构之后只就会执行我们的删除器代码。 
删除器要做的事情就是将此连接重新加入 connectionQueue 中，然后重新设置这个连接的时间戳。
*/
std::shared_ptr<MysqlConn> ConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> locker(mutex_);
    if (connectionQueue_.empty())
    {
        while (connectionQueue_.empty())
        {
            // 如果为空，需要阻塞一段时间，等待新的可用连接
            if (std::cv_status::timeout == cond_.wait_for(locker, std::chrono::milliseconds(timeout_)))
            {
                // std::cv_status::timeout 表示超时
                if (connectionQueue_.empty())
                {
                    continue;
                }
            }
        }
    }
    
    // 有可用的连接
    // 如何还回数据库连接？
    // 使用共享智能指针并规定其删除器
    // 规定销毁后调用删除器，在互斥的情况下更新空闲时间并加入数据库连接池,
    // 所以连接并没有真正销毁
    std::shared_ptr<MysqlConn> connptr(connectionQueue_.front(), 
        [this](MysqlConn* conn) {
            std::lock_guard<std::mutex> locker(mutex_);
            conn->refreshAliveTime();
            connectionQueue_.push(conn);
        });
    connectionQueue_.pop();
    cond_.notify_all();
    return connptr;
}