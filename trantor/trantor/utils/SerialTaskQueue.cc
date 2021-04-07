/**
 *
 *  SerialTaskQueue.cc
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

#include <trantor/utils/SerialTaskQueue.h>
#include <trantor/utils/Logger.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
namespace trantor
{
SerialTaskQueue::SerialTaskQueue(const std::string &name)
    : queueName_(name.empty() ? "SerailTaskQueue" : name),
      loopThread_(queueName_)
{
    loopThread_.run();
}
void SerialTaskQueue::stop()
{
    stop_ = true;
    loopThread_.getLoop()->quit();
    loopThread_.wait();
}
SerialTaskQueue::~SerialTaskQueue()
{
    if (!stop_)
        stop();
    LOG_TRACE << "destruct SerialTaskQueue('" << queueName_ << "')";
}
void SerialTaskQueue::runTaskInQueue(const std::function<void()> &task)
{
    loopThread_.getLoop()->runInLoop(task);
}
void SerialTaskQueue::runTaskInQueue(std::function<void()> &&task)
{
    loopThread_.getLoop()->runInLoop(std::move(task));
}

void SerialTaskQueue::waitAllTasksFinished()
{
    syncTaskInQueue([]() {

    });
}

}  // namespace trantor
