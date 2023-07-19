#ifndef HTTP_HTTPRESPONSE_H
#define HTTP_HTTPRESPONSE_H

#include <unordered_map>

class Buffer;
class HttpResponse
{
public:
    // 响应状态码
    enum HttpStatusCode
    {
        kUnknown,
        k200Ok = 200,
        k204NoContent = 204,
        k206Partitial = 206,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k404NotFound = 404,
        k500InternalError = 500
    };  

    explicit HttpResponse(bool close)
      : statusCode_(kUnknown),
        closeConnection_(close),
        fd_(-1)
    {
    }   

    void setStatusCode(HttpStatusCode code)
    { statusCode_ = code; } 

    void setStatusMessage(const std::string& message)
    { statusMessage_ = message; }   

    void setCloseConnection(bool on)
    { closeConnection_ = on; }  

    bool closeConnection() const
    { return closeConnection_; }  

    void setContentType(const std::string& contentType)
    { addHeader("Content-Type", contentType); } 

    void setDisposition(const std::string& file)
    { 
        std::string temp = "attachment;filename=";
        temp += file;
        addHeader("Content-Disposition", temp); 
    } 

    void addHeader(const std::string& key, const std::string& value)
    { headers_[key] = value; }  

    void setBody(const std::string& body)
    { body_ = body; }   

    void appendToBuffer(Buffer* output) const;

    bool needSendFile() const { return fd_ != -1; }
            int getFd() const { return fd_; }
            void setFd(int fd)
            {
                // assert(fd >= 0);
                fd_ = fd;
            }
            off64_t getSendLen() const { return len_; }
            void setSendLen(off64_t len)
            {
                // assert(len > 0);
                len_ = len;
            }

private:
    std::unordered_map<std::string, std::string> headers_;
    HttpStatusCode statusCode_;
    // FIXME: add http version
    std::string statusMessage_;
    bool closeConnection_;               // 是否关闭长连接
    std::string body_;
    int fd_;                           // 需要传输文件时使用
    off64_t len_;                          // 传输大小
};

#endif // HTTP_HTTPRESPONSE_H