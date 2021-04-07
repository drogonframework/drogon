/**
 *
 *  @file TaskQueue.h
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

#pragma once

#include "NonCopyable.h"
#include <functional>
#include <future>
#include <string>
namespace trantor
{
/**
 * @brief This class is a pure virtual class that can be implemented as a
 * SerialTaskQueue or a ConcurrentTaskQueue.
 *
 */
class TaskQueue : public NonCopyable
{
  public:
    virtual void runTaskInQueue(const std::function<void()> &task) = 0;
    virtual void runTaskInQueue(std::function<void()> &&task) = 0;
    virtual std::string getName() const
    {
        return "";
    };

    /**
     * @brief Run a task in the queue sychronously. This means that the task is
     * executed before the method returns.
     *
     * @param task
     */
    void syncTaskInQueue(const std::function<void()> &task)
    {
        std::promise<int> prom;
        std::future<int> fut = prom.get_future();
        runTaskInQueue([&]() {
            task();
            prom.set_value(1);
        });
        fut.get();
    };
    virtual ~TaskQueue()
    {
    }
};
}  // namespace trantor
