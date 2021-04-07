/**
 *
 *  MsgBuffer.cc
 *  An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#include <trantor/utils/MsgBuffer.h>
#include <trantor/utils/Funcs.h>
#include <string.h>
#ifndef _WIN32
#include <sys/uio.h>
#include <netinet/in.h>
#else
#include <WindowsSupport.h>
#include <winsock2.h>
#endif
#include <errno.h>
#include <assert.h>

using namespace trantor;
namespace trantor
{
static constexpr size_t kBufferOffset{8};
}

MsgBuffer::MsgBuffer(size_t len)
    : head_(kBufferOffset), initCap_(len), buffer_(len + head_), tail_(head_)
{
}

void MsgBuffer::ensureWritableBytes(size_t len)
{
    if (writableBytes() >= len)
        return;
    if (head_ + writableBytes() >=
        (len + kBufferOffset))  // move readable bytes
    {
        std::copy(begin() + head_, begin() + tail_, begin() + kBufferOffset);
        tail_ = kBufferOffset + (tail_ - head_);
        head_ = kBufferOffset;
        return;
    }
    // create new buffer
    size_t newLen;
    if ((buffer_.size() * 2) > (kBufferOffset + readableBytes() + len))
        newLen = buffer_.size() * 2;
    else
        newLen = kBufferOffset + readableBytes() + len;
    MsgBuffer newbuffer(newLen);
    newbuffer.append(*this);
    swap(newbuffer);
}
void MsgBuffer::swap(MsgBuffer &buf) noexcept
{
    buffer_.swap(buf.buffer_);
    std::swap(head_, buf.head_);
    std::swap(tail_, buf.tail_);
    std::swap(initCap_, buf.initCap_);
}
void MsgBuffer::append(const MsgBuffer &buf)
{
    ensureWritableBytes(buf.readableBytes());
    memcpy(&buffer_[tail_], buf.peek(), buf.readableBytes());
    tail_ += buf.readableBytes();
}
void MsgBuffer::append(const char *buf, size_t len)
{
    ensureWritableBytes(len);
    memcpy(&buffer_[tail_], buf, len);
    tail_ += len;
}
void MsgBuffer::appendInt16(const uint16_t s)
{
    uint16_t ss = htons(s);
    append(static_cast<const char *>((void *)&ss), 2);
}
void MsgBuffer::appendInt32(const uint32_t i)
{
    uint32_t ii = htonl(i);
    append(static_cast<const char *>((void *)&ii), 4);
}
void MsgBuffer::appendInt64(const uint64_t l)
{
    uint64_t ll = hton64(l);
    append(static_cast<const char *>((void *)&ll), 8);
}

void MsgBuffer::addInFrontInt16(const uint16_t s)
{
    uint16_t ss = htons(s);
    addInFront(static_cast<const char *>((void *)&ss), 2);
}
void MsgBuffer::addInFrontInt32(const uint32_t i)
{
    uint32_t ii = htonl(i);
    addInFront(static_cast<const char *>((void *)&ii), 4);
}
void MsgBuffer::addInFrontInt64(const uint64_t l)
{
    uint64_t ll = hton64(l);
    addInFront(static_cast<const char *>((void *)&ll), 8);
}

uint16_t MsgBuffer::peekInt16() const
{
    assert(readableBytes() >= 2);
    uint16_t rs = *(static_cast<const uint16_t *>((void *)peek()));
    return ntohs(rs);
}
uint32_t MsgBuffer::peekInt32() const
{
    assert(readableBytes() >= 4);
    uint32_t rl = *(static_cast<const uint32_t *>((void *)peek()));
    return ntohl(rl);
}
uint64_t MsgBuffer::peekInt64() const
{
    assert(readableBytes() >= 8);
    uint64_t rll = *(static_cast<const uint64_t *>((void *)peek()));
    return ntoh64(rll);
}

void MsgBuffer::retrieve(size_t len)
{
    if (len >= readableBytes())
    {
        retrieveAll();
        return;
    }
    head_ += len;
}
void MsgBuffer::retrieveAll()
{
    if (buffer_.size() > (initCap_ * 2))
    {
        buffer_.resize(initCap_);
    }
    tail_ = head_ = kBufferOffset;
}
ssize_t MsgBuffer::readFd(int fd, int *retErrno)
{
    char extBuffer[8192];
    struct iovec vec[2];
    size_t writable = writableBytes();
    vec[0].iov_base = begin() + tail_;
    vec[0].iov_len = static_cast<int>(writable);
    vec[1].iov_base = extBuffer;
    vec[1].iov_len = sizeof(extBuffer);
    const int iovcnt = (writable < sizeof extBuffer) ? 2 : 1;
    ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *retErrno = errno;
    }
    else if (static_cast<size_t>(n) <= writable)
    {
        tail_ += n;
    }
    else
    {
        tail_ = buffer_.size();
        append(extBuffer, n - writable);
    }
    return n;
}

std::string MsgBuffer::read(size_t len)
{
    if (len > readableBytes())
        len = readableBytes();
    std::string ret(peek(), len);
    retrieve(len);
    return ret;
}
uint8_t MsgBuffer::readInt8()
{
    uint8_t ret = peekInt8();
    retrieve(1);
    return ret;
}
uint16_t MsgBuffer::readInt16()
{
    uint16_t ret = peekInt16();
    retrieve(2);
    return ret;
}
uint32_t MsgBuffer::readInt32()
{
    uint32_t ret = peekInt32();
    retrieve(4);
    return ret;
}
uint64_t MsgBuffer::readInt64()
{
    uint64_t ret = peekInt64();
    retrieve(8);
    return ret;
}

void MsgBuffer::addInFront(const char *buf, size_t len)
{
    if (head_ >= len)
    {
        memcpy(begin() + head_ - len, buf, len);
        head_ -= len;
        return;
    }
    if (len <= writableBytes())
    {
        std::copy(begin() + head_, begin() + tail_, begin() + head_ + len);
        memcpy(begin() + head_, buf, len);
        tail_ += len;
        return;
    }
    size_t newLen;
    if (len + readableBytes() < initCap_)
        newLen = initCap_;
    else
        newLen = len + readableBytes();
    MsgBuffer newBuf(newLen);
    newBuf.append(buf, len);
    newBuf.append(*this);
    swap(newBuf);
}
