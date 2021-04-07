/**
 *
 *  Acceptor.cc
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

#include "Acceptor.h"
using namespace trantor;

#ifndef O_CLOEXEC
#define O_CLOEXEC O_NOINHERIT
#endif

Acceptor::Acceptor(EventLoop *loop,
                   const InetAddress &addr,
                   bool reUseAddr,
                   bool reUsePort)

    :
#ifndef _WIN32
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)),
#endif
      sock_(
          Socket::createNonblockingSocketOrDie(addr.getSockAddr()->sa_family)),
      addr_(addr),
      loop_(loop),
      acceptChannel_(loop, sock_.fd())
{
    sock_.setReuseAddr(reUseAddr);
    sock_.setReusePort(reUsePort);
    sock_.bindAddress(addr_);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::readCallback, this));
    if (addr_.toPort() == 0)
    {
        addr_ = InetAddress{Socket::getLocalAddr(sock_.fd())};
    }
}
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
#ifndef _WIN32
    ::close(idleFd_);
#endif
}
void Acceptor::listen()
{
    loop_->assertInLoopThread();
    sock_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::readCallback()
{
    InetAddress peer;
    int newsock = sock_.accept(&peer);
    if (newsock >= 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(newsock, peer);
        }
        else
        {
#ifndef _WIN32
            ::close(newsock);
#else
            closesocket(newsock);
#endif
        }
    }
    else
    {
        LOG_SYSERR << "Accpetor::readCallback";
// Read the section named "The special problem of
// accept()ing when you can't" in libev's doc.
// By Marc Lehmann, author of libev.
/// errno is thread safe
#ifndef _WIN32
        if (errno == EMFILE)
        {
            ::close(idleFd_);
            idleFd_ = sock_.accept(&peer);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
#endif
    }
}
