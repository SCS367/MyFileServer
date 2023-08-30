# MyFileServer
本项目通过参考muduo实现了一个基于Reactor模型的多线程网络库，并使用C++11去除了muduo对于Boost库的依赖。在此基础上实现了一个简单的
文件管理服务器，可对指定目录下的所有文件进行管理， 支持用户的注册、登录，文件的上传、下载，删除等功能。  
获取当前url下的所有文件:
![获取当前url下的所有文件](https://github.com/SCS367/MyFileServer/blob/main/%E9%A2%84%E8%A7%88.jpg)  
![获取当前url下的所有文件](https://github.com/SCS367/MyFileServer/blob/main/%E5%9B%BE%E7%89%87.jpg)
上传文件：
![获取当前url下的所有文件](https://github.com/SCS367/MyFileServer/blob/main/%E4%B8%8A%E4%BC%A0.jpg)
下载文件：
![获取当前url下的所有文件](https://github.com/SCS367/MyFileServer/blob/main/%E4%B8%8B%E8%BD%BD.jpg)
