/**
 *
 *  Socket.h
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

#include <trantor/utils/NonCopyable.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Logger.h>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>

namespace trantor
{
class Socket : NonCopyable
{
  public:
    static int createNonblockingSocketOrDie(int family)
    {
#ifdef __linux__
        int sock = ::socket(family,
                            SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                            IPPROTO_TCP);
#else
        int sock = static_cast<int>(::socket(family, SOCK_STREAM, IPPROTO_TCP));
        setNonBlockAndCloseOnExec(sock);
#endif
        if (sock < 0)
        {
            LOG_SYSERR << "sockets::createNonblockingOrDie";
            exit(1);
        }
        LOG_TRACE << "sock=" << sock;
        return sock;
    }

    static int getSocketError(int sockfd)
    {
        int optval;
        socklen_t optlen = static_cast<socklen_t>(sizeof optval);
#ifdef _WIN32
        if (::getsockopt(
                sockfd, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen) < 0)
#else
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
#endif
        {
            return errno;
        }
        else
        {
            return optval;
        }
    }

    static int connect(int sockfd, const InetAddress &addr)
    {
        if (addr.isIpV6())
            return ::connect(sockfd,
                             addr.getSockAddr(),
                             static_cast<socklen_t>(
                                 sizeof(struct sockaddr_in6)));
        else
            return ::connect(sockfd,
                             addr.getSockAddr(),
                             static_cast<socklen_t>(
                                 sizeof(struct sockaddr_in)));
    }

    static bool isSelfConnect(int sockfd);

    explicit Socket(int sockfd) : sockFd_(sockfd)
    {
    }
    ~Socket();
    /// abort if address in use
    void bindAddress(const InetAddress &localaddr);
    /// abort if address in use
    void listen();
    int accept(InetAddress *peeraddr);
    void closeWrite();
    int read(char *buffer, uint64_t len);
    int fd()
    {
        return sockFd_;
    }
    static struct sockaddr_in6 getLocalAddr(int sockfd);
    static struct sockaddr_in6 getPeerAddr(int sockfd);

    ///
    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    ///
    void setTcpNoDelay(bool on);

    ///
    /// Enable/disable SO_REUSEADDR
    ///
    void setReuseAddr(bool on);

    ///
    /// Enable/disable SO_REUSEPORT
    ///
    void setReusePort(bool on);

    ///
    /// Enable/disable SO_KEEPALIVE
    ///
    void setKeepAlive(bool on);
    int getSocketError();

  protected:
    int sockFd_;

  public:
    // taken from muduo
    static void setNonBlockAndCloseOnExec(int sockfd)
    {
#ifdef _WIN32
        // TODO how to set FD_CLOEXEC on windows? is it necessary?
        u_long arg = 1;
        auto ret = ioctlsocket(sockfd, (long)FIONBIO, &arg);
        if (ret)
        {
            LOG_ERROR << "ioctlsocket error";
        }
#else
        // non-block
        int flags = ::fcntl(sockfd, F_GETFL, 0);
        flags |= O_NONBLOCK;
        int ret = ::fcntl(sockfd, F_SETFL, flags);
        // TODO check

        // close-on-exec
        flags = ::fcntl(sockfd, F_GETFD, 0);
        flags |= FD_CLOEXEC;
        ret = ::fcntl(sockfd, F_SETFD, flags);
        // TODO check

        (void)ret;
#endif
    }
};

}  // namespace trantor
