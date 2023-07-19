#include "FileServer.h"

#include "Logging.h"
#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

#include <sys/stat.h>
#include <cmath>
#include <sstream>
#include <dirent.h>
#include <fcntl.h>
#include <vector>
#include <functional>
#include<fstream>

using namespace std;
void getFileListPage(std::string &fileListHtml, std::string &workPath_, std::string &m_url);
void getFileListPage(std::string &fileListHtml, std::string &workPath_);

void getFileVec(const std::string dirName, std::vector<std::string> &resVec);

            string get404Html(string &msg)
            {
                return R"(<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
    <head>
        <meta http-equiv="Content-Type" content="text/html;charset=utf-8">
        <title>Error response</title>
    </head>
    <body>
        <h1>Error response</h1>
        <p>Error code: 404</p>
        <p>Message:)" + msg +
                       R"(</p>
        <p>Error code explanation: HTTPStatus.NOT_FOUND - Nothing matches the given URI.</p>
    </body>
</html>
)";
            }

            void scanDir(const string &path, string &list)
            {
                // <li><a href="branches/">branches/</a></li>
                struct stat buffer;
                struct dirent *dirp; // return value for readdir()
                DIR *dir;            // return value for opendir()

                char fullname[100];     // to store files full name
                size_t n = path.size(); // fullname[n]=='\0'
                strcpy(fullname, path.c_str());
                if (fullname[n - 1] != '/')
                {
                    fullname[n] = '/';
                    fullname[n + 1] = '\0';
                    n++;
                }

                dir = opendir(path.c_str());
                if (!dir)
                    return;
                while ((dirp = readdir(dir)))
                {
                    if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
                        continue;
                    strcpy(&fullname[n], dirp->d_name);
                    stat(fullname, &buffer);

                    char name[600];
                    if (S_ISDIR(buffer.st_mode))
                        snprintf(name, sizeof(name), "<li><a href=\"%s/\">%s/</a></li>", dirp->d_name, dirp->d_name);
                    else
                        snprintf(name, sizeof(name), "<li><a href=\"%s\">%s</a></li>", dirp->d_name, dirp->d_name);
                    list.append(name);
                    list.push_back('\n');
                }
                closedir(dir);
            }


pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;
std::map<std::string, std::string> MimeType::mime;

void FileServer::sql_pool()
{
    //初始化数据库读取表
    conn = m_connPool->getConnection();
    if(conn != nullptr) std::cout<<"conn success"<<std::endl; else std::cout<<"connect failed"<<std::endl;
    MYSQL_RES* result = conn->query("SELECT user, password FROM login");
    if(result == nullptr) std::cout<<"sql_pool error"<<std::endl;

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

void MimeType::init()
{
    mime[".html"] = "text/html;charset=utf-8";
    mime[".avi"] = "video/avi";
    mime[".flv"] = "video/flv";
    mime[".mp4"] = "video/mp4";
    mime[".bmp"] = "image/bmp";
    mime[".doc"] = "application/msword";
    mime[".gif"] = "image/gif";
    mime[".gz"] = "application/x-gzip";
    mime[".htm"] = "text/html;charset=utf-8";
    mime[".ico"] = "image/x-icon";
    mime[".jpg"] = "image/jpeg";
    mime[".png"] = "image/png";
    mime[".txt"] = "text/plain;charset=utf-8";
    mime[".mp3"] = "audio/mp3";
    mime[".cc"] = "text/plain;charset=utf-8";
    mime[".hpp"] = "text/plain;charset=utf-8";
    mime[".h"] = "text/plain;charset=utf-8";
    mime[".cpp"] = "text/plain;charset=utf-8";
    mime[".c"] = "text/plain;charset=utf-8";
    mime[".sh"] = "text/plain;charset=utf-8";
    mime[".py"] = "text/plain;charset=utf-8";
    mime[".md"] = "text/plain;charset=utf-8";
    mime["default"] = "text/html;charset=utf-8";
}

string MimeType::getMime(const std::string &suffix)
{
    pthread_once(&once_control, MimeType::init);
    if (mime.find(suffix) == mime.end())
        return mime["default"];
    else
        return mime[suffix];
}

FileServer::FileServer(const string &path,
                       EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &name,
                       TcpServer::Option option)
    : workPath_(path),
      server_(loop, listenAddr, name, option)
{
    server_.setConnectionCallback(std::bind(&FileServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(std::bind(&FileServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    server_.setThreadNum(8);
    m_connPool = ConnectionPool::getConnectionPool();
}

void FileServer::start()
{
    LOG_WARN << "FileServer[" << server_.name()
             << "] starts listening on " << server_.ipPort();
    server_.start();
}

void FileServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        conn->setContext(HttpContext());
    }
}

void FileServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp receiveTime)
{
    HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());
    // std::cout<<"浏览器发来的请求报文："<<std::endl;
    // std::cout<<buf->GetBufferAllAsString()<<endl;
    if (!context->parseRequest(buf, receiveTime))  //反序列化（解析）为request
    {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }
    if (context->gotAll())
    {
        onRequest(conn, context->request());
        context->reset();
    }
}

extern char favicon[555];
void FileServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    LOG_WARN << "Request : " << req.methodString() << " " << req.path();
    if (req.getVersion() == HttpRequest::kHttp10)
        LOG_WARN << "Http 1.0";
    else
        LOG_WARN << "Http 1.1";
    const string &connection = req.getHeader("Connection");
    bool close = connection == "close" ||
                 (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);

    // 网站图标
    if (req.path() == "/favicon.ico")
    {
        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType("image/png");
        response.setBody(string(favicon, sizeof favicon));
    }
    else
        setResponseBody(req, response);

    Buffer buf;
    response.appendToBuffer(&buf);
    // std::cout<<"buffer中的数据"<<std::endl;
    // std::string request = buf.GetBufferAllAsString();
    // std::cout << request << std::endl;
    conn->send(&buf);
    if (response.needSendFile())
    {
        // 需要发送文件
        int fd = response.getFd();
        size_t needLen = static_cast<size_t>(response.getSendLen());
        // std::cout<<"onRequest"<<std::endl;
        // std::cout<<fd<<" "<<needLen<<std::endl;
        conn->sendFile(fd, needLen);
    }

    if (response.closeConnection())
    {
        conn->shutdown();
    }
}

void FileServer::setResponseBody(const HttpRequest &req, HttpResponse &res)
{
    // static const off64_t maxSendLen = 1024 * 1024 * 100;
    string path = workPath_ + req.path();
    string m_url = req.path();
    // if(m_url == "/") m_url = path;
    struct stat buffer;
    // std::cout<<"path:"<<path<<std::endl;
    // std::cout<<"m_url:"<<m_url<<std::endl;
    
    if (stat(path.c_str(), &buffer) == 0)  //返回-1，文件不存在
    {
        if (S_ISDIR(buffer.st_mode))
        { // 目录
            std::cout<<" 目录文件"<<std::endl;
            //TODO 
            if (strlen(m_url.c_str()) == 1) {
                string html;
                html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>WebServer</title>
    </head>
    <body>
    <br/>
    <br/>
    <div align="center"><font size="5"> <strong>欢迎访问</strong></font></div>
	<br/>
		<br/>
		<form action="0" method="post">
 			<div align="center"><button type="submit">新用户</button></div>
                </form>
		<br/>
                <form action="1" method="post">
                        <div align="center"><button type="submit" >已有账号</button></div>
                </form>
		
		
        </div>
    </body>
</html>)";

                res.setStatusCode(HttpResponse::k200Ok);
                res.setContentType("text/html;charset=utf-8");
                res.setBody(html);
            } else{ 
            string list, show_path = path[0] == '.' ? path.substr(1) : path;
            // std::cout<<" list before scan "<<list<<std::endl;
            scanDir(path, list);
            // std::cout<<"show_path "<<show_path<<std::endl<<" list "<<list<<std::endl;
            string html;
            getFileListPage(html, show_path, m_url);
            res.setStatusCode(HttpResponse::k200Ok);
            res.setContentType("text/html;charset=utf-8");
            res.setBody(html);
            }
        }
        else if (S_ISREG(buffer.st_mode))
        { // 常规文件
            std::cout<<" 常规文件"<<std::endl;
            int fd = ::open(path.c_str(), O_RDONLY);

            // off64_t len = lseek(fd, 0, SEEK_END) - lseek(fd, 0, SEEK_SET);
            off64_t len = buffer.st_size;
            res.setFd(fd);

            string suffix;
            size_t pos = req.path().find_last_of('.');
            if (pos != req.path().npos)
                suffix = req.path().substr(pos);
            else
                suffix = "";
            LOG_DEBUG << "File suffix: " << suffix;
            std::cout<<"suffix"<<suffix<<std::endl;
            string type = MimeType::getMime(suffix);
            res.setContentType(type);
            res.addHeader("Accept-Ranges", "bytes");

            string range = req.getHeader("Range");
            // cout<<"range"<<endl;
            // cout<<range<<endl;
            if (range != "")
            {
                LOG_INFO << "使用range";
                res.setStatusCode(HttpResponse::k206Partitial);
                res.setStatusMessage("Partial Content");

                off64_t beg_num = 0, end_num = 0;
                string range_value = req.getHeader("Range").substr(6);
                pos = range_value.find("-");
                string beg = range_value.substr(0, pos);
                string end = range_value.substr(pos + 1);
                if (beg != "" && end != "")
                {
                    beg_num = stoi(beg);
                    end_num = stoi(end);
                }
                else if (beg != "" && end == "")
                {
                    beg_num = stoi(beg);
                    end_num = len - 1;
                }
                else if (beg == "" && end != "")
                {
                    beg_num = len - stoi(end);
                    end_num = len - 1;
                }

                // 需要读need_len个字节
                // off64_t need_len = std::min(end_num - beg_num + 1, maxSendLen);
                off64_t need_len = end_num - beg_num + 1;
                end_num = beg_num + need_len - 1;
                lseek(fd, beg_num, SEEK_SET);
                res.setSendLen(static_cast<int>(need_len));

                std::ostringstream os_range;
                os_range << "bytes " << beg_num << "-" << end_num << "/" << len;
                res.addHeader("Content-Range", os_range.str());
                res.addHeader("Content-Length", std::to_string(need_len));
                std::cout<<"os_range "<<os_range.str()<<std::endl;
                LOG_INFO << os_range.str();
            }
            else
            {
                std::cout<<" 非range传送"<<std::endl;
                res.setStatusCode(HttpResponse::k200Ok);
                res.setStatusMessage("OK");
                res.setSendLen(len);
                res.addHeader("Content-Length", std::to_string(len));
            }
        }
        else
        {
            // 非常规文件
            res.setStatusCode(HttpResponse::k404NotFound);
            res.setContentType("text/html;charset=utf-8");
            string msg = "This is not a regular file.";
            std::cout<<msg<<std::endl;
            res.setBody(get404Html(msg));
        }
    } else if (m_url[1] == '0')
    {
        string html;
                html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>Sign up</title>
    </head>
    <body>
<br/>
<br/>
    <div align="center"><font size="5"> <strong>注册</strong></font></div>
    <br/>
        <div class="login">
                <form action="3CGISQL.cgi" method="post">
                        <div align="center"><input type="text" name="user" placeholder="用户名" required="required"></div><br/>
                        <div align="center"><input type="password" name="password" placeholder="用户密码" required="required"></div><br/>
                        <div align="center"><button type="submit">注册</button></div>
                </form>
        </div>
    </body>
</html>)";
                // std::cout<<"html:"<<html<<std::endl;

                res.setStatusCode(HttpResponse::k200Ok);
                res.setContentType("text/html;charset=utf-8");
                res.setBody(html);
    } else if (m_url[1] == '1') {
        string html;
                html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>Sign in</title>
    </head>
    <body>
<br/>
<br/>
    <div align="center"><font size="5"> <strong>登录</strong></font></div>
    <br/>
        <div class="login">
                <form action="2CGISQL.cgi" method="post">
                        <div align="center"><input type="text" name="user" placeholder="用户名" required="required"></div><br/>
                        <div align="center"><input type="password" name="password" placeholder="登录密码" required="required"></div><br/>
                        <div align="center"><button type="submit">确定</button></div>
                </form>
        </div>
    </body>
</html>)";
                //std::cout<<"html:"<<html<<std::endl;

                res.setStatusCode(HttpResponse::k200Ok);
                res.setContentType("text/html;charset=utf-8");
                res.setBody(html);
    } else if (m_url[1] == '2' || m_url[1] == '3') {
        // std::cout << users.size() <<std::endl;
        // for(auto it : users) {
        //     cout << it.first <<" "<< it.second <<std::endl;
        // }
        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        //std::cout<<"将用户名和密码提取出来 "<<req.m_string<<std::endl;
        for (i = 5; req.m_string[i] != '&'; ++i)
            name[i - 5] = req.m_string[i];
        name[i - 5] = '\0';
        //std::cout<<"用户名"<<name<<std::endl;

        int j = 0;
        for (i = i + 10; req.m_string[i] != '\0'; ++i, ++j)
            password[j] = req.m_string[i];
        password[j] = '\0';
        //std::cout<<"密码"<<password<<std::endl;

        if(m_url[1] == '3') {
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO login(user, password) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            bool ins;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ins = conn->update(sql_insert);
                users.insert(pair<string, string>(name, password));
            }
            string html;
                if (ins) {
                html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>Sign in</title>
    </head>
    <body>
<br/>
<br/>
    <div align="center"><font size="5"> <strong>登录</strong></font></div>
    <br/>
        <div class="login">
                <form action="2CGISQL.cgi" method="post">
                        <div align="center"><input type="text" name="user" placeholder="用户名" required="required"></div><br/>
                        <div align="center"><input type="password" name="password" placeholder="登录密码" required="required"></div><br/>
                        <div align="center"><button type="submit">确定</button></div>
                </form>
        </div>
    </body>
</html>)";
                }
                    
                else {
                html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>Sign up</title>
    </head>
    <body>
<br/>
<br/>
    <div align="center"><font size="5"> <strong>注册</strong></font></div>
    <br/>
        <div class="login">
                <form action="3CGISQL.cgi" method="post">
                        <div align="center"><input type="text" name="user" placeholder="用户名" required="required"></div><br/>
                        <div align="center"><input type="password" name="password" placeholder="用户密码" required="required"></div><br/>
                        <div align="center"><button type="submit">注册</button></div>
                </form>
		<div  align="center">提示：该用户名被注册.</div>
        </div>
    </body>
</html>)";  
                    
            }
            res.setStatusCode(HttpResponse::k200Ok);
            res.setContentType("text/html;charset=utf-8");
            res.setBody(html);
        }

        else if(m_url[1] == '2') {
            string html;
            //std::cout << "m_url[1] == '2'" <<std::endl;
            if (users.find(name) != users.end() && users[name] == password) {
            // string path = workPath_;
            // string list, html, show_path = path[0] == '.' ? path.substr(1) : path;
            // std::cout<< "path:" << path <<std::endl;
            // std::cout<< show_path <<std::endl;
            // scanDir(path, list);
            getFileListPage(html, workPath_);
            //std::cout<< "html:" << html <<std::endl;
            }
            else {
                html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>Sign in</title>
    </head>
    <body>
<br/>
<br/>
    <div align="center"><font size="5"> <strong>登录</strong></font></div>
    <br/>
        <div class="login">
                <form action="2CGISQL.cgi" method="post">
                        <div align="center"><input type="text" name="user" placeholder="用户名" required="required"></div><br/>
                        <div align="center"><input type="password" name="password" placeholder="登录密码" required="required"></div><br/>
                        <div align="center"><button type="submit">确定</button></div>
                </form>
		<br/>
               <div  align="center">提示：用户名或密码错误，请重试</div>
        </div>
    </body>
</html>)";
            }
            res.setStatusCode(HttpResponse::k200Ok);
            res.setContentType("text/html;charset=utf-8");
            res.setBody(html);

        }
        
    } else if (m_url[1] == 'd' && m_url[2] == 'o') {
        struct stat buffer;
        // std::cout<<" 常规文件"<<std::endl;
        // std::cout<<path<<std::endl;
        // std::cout<<m_url<<std::endl;
        int n_d = path.find("download");
        string dl_path = path.substr(0, n_d);
        string dl_url = m_url.substr(10);
        string download_path = dl_path + dl_url;
        stat(download_path.c_str(), &buffer);
        std::cout<<download_path<<std::endl;
        int fd = ::open(download_path.c_str(), O_RDONLY);

        // off64_t len = lseek(fd, 0, SEEK_END) - lseek(fd, 0, SEEK_SET);
        off64_t len = buffer.st_size;
        res.setFd(fd);

        string suffix;
        size_t pos = req.path().find_last_of('.');
        size_t f_pos = download_path.find_last_of('/');
        string f_name = download_path.substr(f_pos + 1);
        std::cout<<f_name<<std::endl;
        if (pos != req.path().npos)
            suffix = req.path().substr(pos);
        else
            suffix = "";
        LOG_DEBUG << "File suffix: " << suffix;
        std::cout<<"suffix"<<suffix<<std::endl;
        string type = MimeType::getMime(suffix);
        res.setContentType(type);
        res.addHeader("Accept-Ranges", "bytes");
        res.setDisposition(f_name);

        string range = req.getHeader("Range");

        std::cout<<" 非range传送"<<std::endl;
        res.setStatusCode(HttpResponse::k200Ok);
        res.setStatusMessage("OK");
        res.setSendLen(len);
        res.addHeader("Content-Length", std::to_string(len));
    } else if (m_url[1] == 'u') {
        std::string body = req.m_string;
        int n = body.find("-----------------");
        std::string content = body.substr(0, n);
        //std::cout<<"文件内容"<<std::endl;
        // std::cout<<content<<std::endl;
        auto headers = req.headers();
        std::cout<<headers["Content-Disposition"]<<std::endl;
        std::string content_type = headers["Content-Type"];
        std::cout<<content_type<<std::endl;
        std::string f_url = path;
        n = path.find(m_url);
        f_url = path.substr(0, n);
        std::cout<<f_url<<std::endl;
        n = headers["Content-Disposition"].find("filename=");
        std::string f_name = headers["Content-Disposition"].substr(n + 10);
        f_name = f_name.substr(0, f_name.size() - 1);
        std::cout<<f_name<<std::endl;
        

        string path_u;
        if(content_type == "image/jpeg" || content_type == "image/png") {
            path = f_url + "/img/" + f_name;
            path_u = f_url + "/img/";
        } else {
                path = f_url + "/text/" + f_name;
                path_u = f_url + "/text/";  
        }
        std::cout<<path<<std::endl;
        ofstream os(path.c_str());     //创建一个文件输出流对象
        if(!os.is_open())
       {
         std::cout<<"failed"<<std::endl;
       }  
        os.write(content.c_str(),content.size());
        os.close();

        string list, show_path = path_u[0] == '.' ? path_u.substr(1) : path_u;
        std::cout<<" list before scan "<<list<<std::endl;
        scanDir(path_u, list);
        std::cout<<"show_path "<<show_path<<std::endl<<" list "<<list<<std::endl;
        string html;
        html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>WebServer</title>
    </head>
    <body>
    <br/>
    <br/>
    <div align="center"><font size="5"> <strong>上传成功</strong></font></div>
	<br/>
		<br/>
		
        </div>
    </body>
</html>)";
        res.setStatusCode(HttpResponse::k200Ok);
        res.setContentType("text/html;charset=utf-8"); 
        res.setBody(html);
    } else if (m_url[1] == 'd' && m_url[2] == 'e') {
        // std::cout<<" 常规文件"<<std::endl;
        // std::cout<<path<<std::endl;
        // std::cout<<m_url<<std::endl;
        int n_d = path.find("delete");
        string dl_path = path.substr(0, n_d);
        string dl_url = m_url.substr(8);
        std::string f_url = path;
        string download_path = dl_path + dl_url;
        stat(download_path.c_str(), &buffer);
        std::cout<<download_path<<std::endl;
        if(remove(download_path.c_str())==0)
        {
            cout<<"删除成功"<<endl;
        }
        else
        {
            cout<<"删除失败"<<endl;
        }

        auto headers = req.headers();
        std::string content_type = headers["Content-Type"];
        std::cout<<content_type<<std::endl;
        int n_u = download_path.find_last_of("/");
        string path_d = download_path.substr(0, n_u + 1);

        std::cout<<path_d<<std::endl;

        string list, show_path = path_d[0] == '.' ? path_d.substr(1) : path_d;
        std::cout<<" list before scan "<<list<<std::endl;
        scanDir(path_d, list);
        std::cout<<"show_path "<<show_path<<std::endl<<" list "<<list<<std::endl;
        string html;
        html = R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <title>WebServer</title>
    </head>
    <body>
    <br/>
    <br/>
    <div align="center"><font size="5"> <strong>删除成功</strong></font></div>
	<br/>
		<br/>
		
        </div>
    </body>
</html>)";
        res.setStatusCode(HttpResponse::k200Ok);
        res.setContentType("text/html;charset=utf-8"); 
        res.setBody(html);
    }
    
    else // 路径不存在，返回 404
    {
        res.setStatusCode(HttpResponse::k404NotFound);
        res.setContentType("text/html;charset=utf-8");
        string msg = "File not found.";
        std::cout<<msg<<std::endl;
        res.setBody(get404Html(msg));
    }
}





// 以下两个函数用来构建文件列表的页面，最终结果保存到 fileListHtml 中
void getFileListPage(std::string &fileListHtml, std::string &workPath_, std::string &m_url){
    // 结果保存到 fileListHtml

    // 将指定目录内的所有文件保存到 fileVec 中
    std::vector<std::string> fileVec;
    getFileVec(workPath_, fileVec);
    
    // 构建页面
    std::ifstream fileListStream("html/filelist.html", std::ios::in); //输入文件流
    std::string tempLine;
    // 首先读取文件列表的 <!--filelist_label--> 注释前的语句
    while(1){
        getline(fileListStream, tempLine);
        if(tempLine == "<!--filelist_label-->"){
            break;
        }
        fileListHtml += tempLine + "\n";
    }

    // 根据如下标签，将将文件夹中的所有文件项添加到返回页面中
    //             <tr><td class="col1">filenamename</td> <td class="col2"><a href="file/filename">下载</a></td> <td class="col3"><a href="delete/filename">删除</a></td></tr>
    for(const auto &filename : fileVec){
        // fileListHtml += "            <tr><td class=\"col1\">" + filename +
        //             "</td> <td class=\"col2\"><a href=\"download/" + filename +
        //             "\">下载</a></td> <td class=\"col3\"><a href=\"delete/" + filename +
        //             "\" onclick=\"return confirmDelete();\">删除</a></td></tr>" + "\n";
        fileListHtml += "            <tr><td class=\"col1\"><a href=" + m_url  + "/" + filename + ">"
                    + filename + "</a></td> <td class=\"col2\"><a href=\"download" + m_url  + "/"  + filename +
                    "\">下载</a></td> <td class=\"col3\"><a href=\"delete" + m_url  + "/" + filename +
                    "\" onclick=\"return confirmDelete();\">删除</a></td></tr>" + "\n";
    }                                 //           <a href=\"%s/\">%s/</a>

    // 将文件列表注释后的语句加入后面
    while(getline(fileListStream, tempLine)){
        fileListHtml += tempLine + "\n";
    }
    
}

void getFileListPage(std::string &fileListHtml, std::string &workPath_){
    // 结果保存到 fileListHtml

    // 将指定目录内的所有文件保存到 fileVec 中
    std::vector<std::string> fileVec;
    getFileVec(workPath_, fileVec);
    
    // 构建页面
    std::ifstream fileListStream("html/filelist.html", std::ios::in); //输入文件流
    std::string tempLine;
    // 首先读取文件列表的 <!--filelist_label--> 注释前的语句
    while(1){
        getline(fileListStream, tempLine);
        if(tempLine == "<!--filelist_label-->"){
            break;
        }
        fileListHtml += tempLine + "\n";
    }

    // 根据如下标签，将将文件夹中的所有文件项添加到返回页面中
    //             <tr><td class="col1">filenamename</td> <td class="col2"><a href="file/filename">下载</a></td> <td class="col3"><a href="delete/filename">删除</a></td></tr>
    for(const auto &filename : fileVec){
        fileListHtml += "            <tr><td class=\"col1\"><a href="  + filename + ">"
                    + filename + "</a></td> <td class=\"col2\"></td> <td class=\"col3\"></td></tr>" + "\n";
        // fileListHtml += "            <tr><td><a href="  + filename + ">"
        //             + filename + "</a></td> </tr>" + "\n";
    }                                 

    // 将文件列表注释后的语句加入后面
    while(getline(fileListStream, tempLine)){
        fileListHtml += tempLine + "\n";
    }
    
}


void getFileVec(const std::string dirName, std::vector<std::string> &resVec){
    // 使用 dirent 获取文件目录下的所有文件
    DIR *dir;   // 目录指针
    dir = opendir(dirName.c_str());
    struct dirent *stdinfo;
    while (1)
    {
        // 获取文件夹中的一个文件
        stdinfo = readdir(dir);
        if (stdinfo == nullptr){
            break;
        }
        resVec.push_back(stdinfo->d_name);
        if(resVec.back() == "." || resVec.back() == ".."){
            resVec.pop_back();
        }
    }
}

char favicon[555] = {
  '\x89', 'P', 'N', 'G', '\xD', '\xA', '\x1A', '\xA',
  '\x0', '\x0', '\x0', '\xD', 'I', 'H', 'D', 'R',
  '\x0', '\x0', '\x0', '\x10', '\x0', '\x0', '\x0', '\x10',
  '\x8', '\x6', '\x0', '\x0', '\x0', '\x1F', '\xF3', '\xFF',
  'a', '\x0', '\x0', '\x0', '\x19', 't', 'E', 'X',
  't', 'S', 'o', 'f', 't', 'w', 'a', 'r',
  'e', '\x0', 'A', 'd', 'o', 'b', 'e', '\x20',
  'I', 'm', 'a', 'g', 'e', 'R', 'e', 'a',
  'd', 'y', 'q', '\xC9', 'e', '\x3C', '\x0', '\x0',
  '\x1', '\xCD', 'I', 'D', 'A', 'T', 'x', '\xDA',
  '\x94', '\x93', '9', 'H', '\x3', 'A', '\x14', '\x86',
  '\xFF', '\x5D', 'b', '\xA7', '\x4', 'R', '\xC4', 'm',
  '\x22', '\x1E', '\xA0', 'F', '\x24', '\x8', '\x16', '\x16',
  'v', '\xA', '6', '\xBA', 'J', '\x9A', '\x80', '\x8',
  'A', '\xB4', 'q', '\x85', 'X', '\x89', 'G', '\xB0',
  'I', '\xA9', 'Q', '\x24', '\xCD', '\xA6', '\x8', '\xA4',
  'H', 'c', '\x91', 'B', '\xB', '\xAF', 'V', '\xC1',
  'F', '\xB4', '\x15', '\xCF', '\x22', 'X', '\x98', '\xB',
  'T', 'H', '\x8A', 'd', '\x93', '\x8D', '\xFB', 'F',
  'g', '\xC9', '\x1A', '\x14', '\x7D', '\xF0', 'f', 'v',
  'f', '\xDF', '\x7C', '\xEF', '\xE7', 'g', 'F', '\xA8',
  '\xD5', 'j', 'H', '\x24', '\x12', '\x2A', '\x0', '\x5',
  '\xBF', 'G', '\xD4', '\xEF', '\xF7', '\x2F', '6', '\xEC',
  '\x12', '\x20', '\x1E', '\x8F', '\xD7', '\xAA', '\xD5', '\xEA',
  '\xAF', 'I', '5', 'F', '\xAA', 'T', '\x5F', '\x9F',
  '\x22', 'A', '\x2A', '\x95', '\xA', '\x83', '\xE5', 'r',
  '9', 'd', '\xB3', 'Y', '\x96', '\x99', 'L', '\x6',
  '\xE9', 't', '\x9A', '\x25', '\x85', '\x2C', '\xCB', 'T',
  '\xA7', '\xC4', 'b', '1', '\xB5', '\x5E', '\x0', '\x3',
  'h', '\x9A', '\xC6', '\x16', '\x82', '\x20', 'X', 'R',
  '\x14', 'E', '6', 'S', '\x94', '\xCB', 'e', 'x',
  '\xBD', '\x5E', '\xAA', 'U', 'T', '\x23', 'L', '\xC0',
  '\xE0', '\xE2', '\xC1', '\x8F', '\x0', '\x9E', '\xBC', '\x9',
  'A', '\x7C', '\x3E', '\x1F', '\x83', 'D', '\x22', '\x11',
  '\xD5', 'T', '\x40', '\x3F', '8', '\x80', 'w', '\xE5',
  '3', '\x7', '\xB8', '\x5C', '\x2E', 'H', '\x92', '\x4',
  '\x87', '\xC3', '\x81', '\x40', '\x20', '\x40', 'g', '\x98',
  '\xE9', '6', '\x1A', '\xA6', 'g', '\x15', '\x4', '\xE3',
  '\xD7', '\xC8', '\xBD', '\x15', '\xE1', 'i', '\xB7', 'C',
  '\xAB', '\xEA', 'x', '\x2F', 'j', 'X', '\x92', '\xBB',
  '\x18', '\x20', '\x9F', '\xCF', '3', '\xC3', '\xB8', '\xE9',
  'N', '\xA7', '\xD3', 'l', 'J', '\x0', 'i', '6',
  '\x7C', '\x8E', '\xE1', '\xFE', 'V', '\x84', '\xE7', '\x3C',
  '\x9F', 'r', '\x2B', '\x3A', 'B', '\x7B', '7', 'f',
  'w', '\xAE', '\x8E', '\xE', '\xF3', '\xBD', 'R', '\xA9',
  'd', '\x2', 'B', '\xAF', '\x85', '2', 'f', 'F',
  '\xBA', '\xC', '\xD9', '\x9F', '\x1D', '\x9A', 'l', '\x22',
  '\xE6', '\xC7', '\x3A', '\x2C', '\x80', '\xEF', '\xC1', '\x15',
  '\x90', '\x7', '\x93', '\xA2', '\x28', '\xA0', 'S', 'j',
  '\xB1', '\xB8', '\xDF', '\x29', '5', 'C', '\xE', '\x3F',
  'X', '\xFC', '\x98', '\xDA', 'y', 'j', 'P', '\x40',
  '\x0', '\x87', '\xAE', '\x1B', '\x17', 'B', '\xB4', '\x3A',
  '\x3F', '\xBE', 'y', '\xC7', '\xA', '\x26', '\xB6', '\xEE',
  '\xD9', '\x9A', '\x60', '\x14', '\x93', '\xDB', '\x8F', '\xD',
  '\xA', '\x2E', '\xE9', '\x23', '\x95', '\x29', 'X', '\x0',
  '\x27', '\xEB', 'n', 'V', 'p', '\xBC', '\xD6', '\xCB',
  '\xD6', 'G', '\xAB', '\x3D', 'l', '\x7D', '\xB8', '\xD2',
  '\xDD', '\xA0', '\x60', '\x83', '\xBA', '\xEF', '\x5F', '\xA4',
  '\xEA', '\xCC', '\x2', 'N', '\xAE', '\x5E', 'p', '\x1A',
  '\xEC', '\xB3', '\x40', '9', '\xAC', '\xFE', '\xF2', '\x91',
  '\x89', 'g', '\x91', '\x85', '\x21', '\xA8', '\x87', '\xB7',
  'X', '\x7E', '\x7E', '\x85', '\xBB', '\xCD', 'N', 'N',
  'b', 't', '\x40', '\xFA', '\x93', '\x89', '\xEC', '\x1E',
  '\xEC', '\x86', '\x2', 'H', '\x26', '\x93', '\xD0', 'u',
  '\x1D', '\x7F', '\x9', '2', '\x95', '\xBF', '\x1F', '\xDB',
  '\xD7', 'c', '\x8A', '\x1A', '\xF7', '\x5C', '\xC1', '\xFF',
  '\x22', 'J', '\xC3', '\x87', '\x0', '\x3', '\x0', 'K',
  '\xBB', '\xF8', '\xD6', '\x2A', 'v', '\x98', 'I', '\x0',
  '\x0', '\x0', '\x0', 'I', 'E', 'N', 'D', '\xAE',
  'B', '\x60', '\x82',
};