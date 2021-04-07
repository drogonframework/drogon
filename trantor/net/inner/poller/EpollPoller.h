/**
 *
 *  EpollPoller.h
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

#pragma once

#include "../Poller.h"
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>

#if defined __linux__ || defined _WIN32
#include <memory>
#include <map>
using EventList = std::vector<struct epoll_event>;
#endif
namespace trantor
{
class Channel;

class EpollPoller : public Poller
{
  public:
    explicit EpollPoller(EventLoop *loop);
    virtual ~EpollPoller();
    virtual void poll(int timeoutMs, ChannelList *activeChannels) override;
    virtual void updateChannel(Channel *channel) override;
    virtual void removeChannel(Channel *channel) override;
#ifdef _WIN32
    virtual void postEvent(uint64_t event) override;
    virtual void setEventCallback(const EventCallback &cb) override
    {
        eventCallback_ = cb;
    }
#endif

  private:
#if defined __linux__ || defined _WIN32
    static const int kInitEventListSize = 16;
#ifdef _WIN32
    void *epollfd_;
    EventCallback eventCallback_{[](uint64_t event) {}};
#else
    int epollfd_;
#endif
    EventList events_;
    void update(int operation, Channel *channel);
#ifndef NDEBUG
    using ChannelMap = std::map<int, Channel *>;
    ChannelMap channels_;
#endif
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
#endif
};
}  // namespace trantor
