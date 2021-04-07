/**
 *
 *  @file EventLoopThread.cc
 *  @author An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#include <trantor/net/EventLoopThread.h>
#include <trantor/utils/Logger.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

using namespace trantor;
EventLoopThread::EventLoopThread(const std::string &threadName)
    : loop_(nullptr),
      loopThreadName_(threadName),
      thread_([this]() { loopFuncs(); })
{
    auto f = promiseForLoopPointer_.get_future();
    loop_ = f.get();
}
EventLoopThread::~EventLoopThread()
{
    run();
    if (loop_)
    {
        loop_->quit();
    }
    if (thread_.joinable())
    {
        thread_.join();
    }
}
// void EventLoopThread::stop() {
//    if(loop_)
//        loop_->quit();
//}
void EventLoopThread::wait()
{
    thread_.join();
}
void EventLoopThread::loopFuncs()
{
#ifdef __linux__
    ::prctl(PR_SET_NAME, loopThreadName_.c_str());
#endif
    EventLoop loop;
    loop.queueInLoop([this]() { promiseForLoop_.set_value(1); });
    promiseForLoopPointer_.set_value(&loop);
    auto f = promiseForRun_.get_future();
    (void)f.get();
    loop.loop();
    // LOG_DEBUG << "loop out";
    loop_ = NULL;
}
void EventLoopThread::run()
{
    std::call_once(once_, [this]() {
        auto f = promiseForLoop_.get_future();
        promiseForRun_.set_value(1);
        // Make sure the event loop loops before returning.
        (void)f.get();
    });
}
