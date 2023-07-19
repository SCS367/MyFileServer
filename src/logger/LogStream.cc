#include "LogStream.h"
#include <algorithm>

static const char digits[] = {'9', '8', '7', '6', '5', '4', '3', '2', '1', '0',
                               '1', '2', '3', '4', '5', '6', '7', '8', '9'};


//对于十进制整型，如int/long，则是通过模板函数formatInteger()，将转换为字符串并直接填入Small Buffer尾部。
//将int等整型转换为string，muduo并没有使用std::to_string，而是使用了效率更高的自定义函数formatInteger()。
template <typename T>
void LogStream::formatInteger(T num)
{
    if (buffer_.avail() >= kMaxNumericSize)  // Small Buffer剩余空间够用
    {
        char* start = buffer_.current();
        char* cur = start;
        const char* zero = digits + 9;
        bool negative = (num < 0); // 是否为负数

        // 末尾取值加入，最后反转
        do {
            int remainder = static_cast<int>(num % 10);
            *(cur++) = zero[remainder];
            num = num / 10;
        } while (num != 0);

        if (negative) {
            *(cur++) = '-';
        }
        *cur = '\0';

        std::reverse(start, cur);
        buffer_.add(static_cast<int>(cur - start)); // cur_向后移动
    }
}

LogStream& LogStream::operator<<(short v)
{
  *this << static_cast<int>(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned short v)
{
    *this << static_cast<unsigned int>(v);
    return *this;
}

LogStream& LogStream::operator<<(int v)
{
    formatInteger<int>(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned int v)
{
    formatInteger<unsigned int>(v);
    return *this;
}

LogStream& LogStream::operator<<(long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(long long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(float v) 
{
    *this << static_cast<double>(v);
    return *this;
}

//对于double类型，使用库函数snprintf转换为const char*，并直接填入Small Buffer尾部。
LogStream& LogStream::operator<<(double v) 
{
    if (buffer_.avail() >= kMaxNumericSize)
    {
        char buf[32];
        int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v); 
        buffer_.add(len);
        return *this;
    }
}

//对于字符类型，跟参数是字符串类型区别是长度只有1，并且无需判断指针是否为空。
LogStream& LogStream::operator<<(char c)
{
    buffer_.append(&c, 1);
    return *this;
}

LogStream& LogStream::operator<<(const void* data) 
{
    *this << static_cast<const char*>(data); 
    return *this;
}
//对于字符串类型参数，operator<<本质上是调用buffer_对应的FixedBuffer<>::append()，将其存放当到Small Buffer中。
LogStream& LogStream::operator<<(const char* str)
{
    if (str)
    {
        buffer_.append(str, strlen(str));
    }
    else 
    {
        buffer_.append("(null)", 6);
    }
    return *this;
}

LogStream& LogStream::operator<<(const unsigned char* str)
{
    return operator<<(reinterpret_cast<const char*>(str));
}

//对于字符串类型参数，operator<<本质上是调用buffer_对应的FixedBuffer<>::append()，将其存放当到Small Buffer中。
LogStream& LogStream::operator<<(const std::string& str)
{
    buffer_.append(str.c_str(), str.size());
    return *this;
}

LogStream& LogStream::operator<<(const Buffer& buf)
{
    *this << buf.toString();
    return *this;
}

LogStream& LogStream::operator<<(const GeneralTemplate& g)
{
    buffer_.append(g.data_, g.len_);
    return *this;
}