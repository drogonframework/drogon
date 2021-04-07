/**
 *
 *  Acceptor.h
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

#include <trantor/net/EventLoop.h>
#include <trantor/utils/NonCopyable.h>
#include "Socket.h"
#include <trantor/net/InetAddress.h>
#include "Channel.h"
#include <functional>

namespace trantor
{
using NewConnectionCallback = std::function<void(int fd, const InetAddress &)>;
class Acceptor : NonCopyable
{
  public:
    Acceptor(EventLoop *loop,
             const InetAddress &addr,
             bool reUseAddr = true,
             bool reUsePort = true);
    ~Acceptor();
    const InetAddress &addr() const
    {
        return addr_;
    }
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    };
    void listen();

  protected:
#ifndef _WIN32
    int idleFd_;
#endif
    Socket sock_;
    InetAddress addr_;
    EventLoop *loop_;
    NewConnectionCallback newConnectionCallback_;
    Channel acceptChannel_;
    void readCallback();
};
}  // namespace trantor
