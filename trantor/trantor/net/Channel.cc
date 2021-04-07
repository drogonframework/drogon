/**
 *
 *  Channel.cc
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

#include "Channel.h"
#include <trantor/net/EventLoop.h>
#ifdef _WIN32
#include "Wepoll.h"
#define POLLIN EPOLLIN
#define POLLPRI EPOLLPRI
#define POLLOUT EPOLLOUT
#define POLLHUP EPOLLHUP
#define POLLNVAL 0
#define POLLERR EPOLLERR
#else
#include <poll.h>
#endif
#include <iostream>
namespace trantor
{
const int Channel::kNoneEvent = 0;

const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

void Channel::remove()
{
    assert(events_ == kNoneEvent);
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::update()
{
    loop_->updateChannel(this);
}

void Channel::handleEvent()
{
    // LOG_TRACE<<"revents_="<<revents_;
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventSafely();
        }
    }
    else
    {
        handleEventSafely();
    }
}
void Channel::handleEventSafely()
{
    if (eventCallback_)
    {
        eventCallback_();
        return;
    }
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
    {
        // LOG_TRACE<<"handle close";
        if (closeCallback_)
            closeCallback_();
    }
    if (revents_ & (POLLNVAL | POLLERR))
    {
        // LOG_TRACE<<"handle error";
        if (errorCallback_)
            errorCallback_();
    }
#ifdef __linux__
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
#else
    if (revents_ & (POLLIN | POLLPRI))
#endif
    {
        // LOG_TRACE<<"handle read";
        if (readCallback_)
            readCallback_();
    }
#ifdef _WIN32
    if ((revents_ & POLLOUT) && !(revents_ & POLLHUP))
#else
    if (revents_ & POLLOUT)
#endif
    {
        // LOG_TRACE<<"handle write";
        if (writeCallback_)
            writeCallback_();
    }
}

}  // namespace trantor
