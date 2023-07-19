#ifndef MYMUDUO_HTTP_FILESERVER_H
#define MYMUDUO_HTTP_FILESERVER_H

#include "TcpServer.h"
#include <map>
#include <string>
#include "ConnectionPool.h"
#include <mutex>
#include <memory>

        class HttpRequest;
        class HttpResponse;

        class MimeType
        {
        private:
            static void init();
            static std::map<std::string, std::string> mime;
            MimeType();
            MimeType(const MimeType &m);

        public:
            static std::string getMime(const std::string &suffix);

        private:
            static pthread_once_t once_control;
        };

        /**
         * 模仿 python http.server 实现的本地文件服务器
         * 支持文件下载范围请求，即 range 首部字段
         * 用户只需要设置工作路径即可
         */
        class FileServer : noncopyable
        {
        public:
            FileServer(const std::string &path,
                       EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &name,
                       TcpServer::Option option = TcpServer::kNoReusePort);

            EventLoop *getLoop() const { return server_.getLoop(); }

            void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
            void start();
            void sql_pool();

        private:
            void onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp receiveTime);
            void onRequest(const TcpConnectionPtr &, const HttpRequest &);
            void onConnection(const TcpConnectionPtr &conn);
            void setResponseBody(const HttpRequest &, HttpResponse &);

            std::string workPath_;
            TcpServer server_;
            //数据库相关
            ConnectionPool *m_connPool;
            int m_sql_num;
            std::mutex mutex_; 
            std::map<std::string, std::string> users;
            std::shared_ptr<MysqlConn> conn;
        };


#endif