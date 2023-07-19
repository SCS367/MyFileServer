#ifndef FIXED_BUFFER_H
#define FIXED_BUFFER_H

#include "noncopyable.h"
#include <assert.h>
#include <string.h> // memcpy
#include <strings.h>
#include <string>

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000*1000; 

template <int SIZE>
class FixedBuffer : noncopyable
{
public:
    FixedBuffer()
        : cur_(data_)
    {
    }

    void append(const char* buf, size_t len) //32位上是unsigned_int,64位上是unsigned_long
    {
        if (static_cast<size_t>(avail()) > len)
        {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }
    //对FixedBuffer<>的各种操作，实际上是对data_数组和cur_指针的操作。
    const char* data() const { return data_; }
    int length() const { return static_cast<int>(end() - data_); }

    char* current() { return cur_; }
    int avail() const { return static_cast<int>(end() - cur_); }
    void add(size_t len) { cur_ += len; }

    void reset() { cur_ = data_; }
    void bzero() { ::bzero(data_, sizeof(data_)); }

    std::string toString() const { return std::string(data_, length()); }

private:
    const char* end() const { return data_ + sizeof(data_); }

    char data_[SIZE];
    char* cur_; 
};

#endif // FIXED_BUFFER_H