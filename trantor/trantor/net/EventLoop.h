// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

// Taken from Muduo and modified
// Copyright 2016, Tao An.  All rights reserved.
// https://github.com/an-tao/trantor
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Tao An

#pragma once
#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/Date.h>
#include <trantor/utils/LockFreeQueue.h>
#include <trantor/exports.h>
#include <thread>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <functional>
#include <chrono>
#include <limits>

namespace trantor
{
class Poller;
class TimerQueue;
class Channel;
using ChannelList = std::vector<Channel *>;
using Func = std::function<void()>;
using TimerId = uint64_t;
enum
{
    InvalidTimerId = 0
};

/**
 * @brief As the name implies, this class represents an event loop that runs in
 * a perticular thread. The event loop can handle network I/O events and timers
 * in asynchronous mode.
 * @note An event loop object always belongs to a separate thread, and there is
 * one event loop object at most in a thread. We can call an event loop object
 * the event loop of the thread it belongs to, or call that thread the thread of
 * the event loop.
 */
class TRANTOR_EXPORT EventLoop : NonCopyable
{
  public:
    EventLoop();
    ~EventLoop();

    /**
     * @brief Run the event loop. This method will be blocked until the event
     * loop exits.
     *
     */
    void loop();

    /**
     * @brief Let the event loop quit.
     *
     */
    void quit();

    /**
     * @brief Assertion that the current thread is the thread to which the event
     * loop belongs. If the assertion fails, the program aborts.
     */
    void assertInLoopThread()
    {
        if (!isInLoopThread())
        {
            abortNotInLoopThread();
        }
    };
#ifdef __linux__
    /**
     * @brief Make the timer queue works after calling the fork() function.
     *
     */
    void resetTimerQueue();
#endif
    /**
     * @brief Make the event loop works after calling the fork() function.
     *
     */
    void resetAfterFork();

    /**
     * @brief Return true if the current thread is the thread to which the event
     * loop belongs.
     *
     * @return true
     * @return false
     */
    bool isInLoopThread() const
    {
        return threadId_ == std::this_thread::get_id();
    };

    /**
     * @brief Get the event loop of the current thread. Return nullptr if there
     * is no event loop in the current thread.
     *
     * @return EventLoop*
     */
    static EventLoop *getEventLoopOfCurrentThread();

    /**
     * @brief Run the function f in the thread of the event loop.
     *
     * @param f
     * @note If the current thread is the thread of the event loop, the function
     * f is executed directly before the method exiting.
     */
    void runInLoop(const Func &f);
    void runInLoop(Func &&f);

    /**
     * @brief Run the function f in the thread of the event loop.
     *
     * @param f
     * @note The difference between this method and the runInLoop() method is
     * that the function f is executed after the method exiting no matter if the
     * current thread is the thread of the event loop.
     */
    void queueInLoop(const Func &f);
    void queueInLoop(Func &&f);

    /**
     * @brief Run a function at a time point.
     *
     * @param time The time to run the function.
     * @param cb The function to run.
     * @return TimerId The ID of the timer.
     */
    TimerId runAt(const Date &time, const Func &cb);
    TimerId runAt(const Date &time, Func &&cb);

    /**
     * @brief Run a function after a period of time.
     *
     * @param delay Represent the period of time in seconds.
     * @param cb The function to run.
     * @return TimerId The ID of the timer.
     */
    TimerId runAfter(double delay, const Func &cb);
    TimerId runAfter(double delay, Func &&cb);

    /**
     * @brief Run a function after a period of time.
     * @note Users could use chrono literals to represent a time duration
     * For example:
     * @code
       runAfter(5s, task);
       runAfter(10min, task);
       @endcode
     */
    TimerId runAfter(const std::chrono::duration<long double> &delay,
                     const Func &cb)
    {
        return runAfter(delay.count(), cb);
    }
    TimerId runAfter(const std::chrono::duration<long double> &delay, Func &&cb)
    {
        return runAfter(delay.count(), std::move(cb));
    }

    /**
     * @brief Repeatedly run a function every period of time.
     *
     * @param interval The duration in seconds.
     * @param cb The function to run.
     * @return TimerId The ID of the timer.
     */
    TimerId runEvery(double interval, const Func &cb);
    TimerId runEvery(double interval, Func &&cb);

    /**
     * @brief Repeatedly run a function every period of time.
     * Users could use chrono literals to represent a time duration
     * For example:
     * @code
       runEvery(5s, task);
       runEvery(10min, task);
       runEvery(0.1h, task);
       @endcode
     */
    TimerId runEvery(const std::chrono::duration<long double> &interval,
                     const Func &cb)
    {
        return runEvery(interval.count(), cb);
    }
    TimerId runEvery(const std::chrono::duration<long double> &interval,
                     Func &&cb)
    {
        return runEvery(interval.count(), std::move(cb));
    }

    /**
     * @brief Invalidate the timer identified by the given ID.
     *
     * @param id The ID of the timer.
     */
    void invalidateTimer(TimerId id);

    /**
     * @brief Move the EventLoop to the current thread, this method must be
     * called before the loop is running.
     *
     */
    void moveToCurrentThread();

    /**
     * @brief Update channel status. This method is usually used internally.
     *
     * @param chl
     */
    void updateChannel(Channel *chl);

    /**
     * @brief Remove a channel from the event loop. This method is usually used
     * internally.
     *
     * @param chl
     */
    void removeChannel(Channel *chl);

    /**
     * @brief Return the index of the event loop.
     *
     * @return size_t
     */
    size_t index()
    {
        return index_;
    }

    /**
     * @brief Set the index of the event loop.
     *
     * @param index
     */
    void setIndex(size_t index)
    {
        index_ = index;
    }

    /**
     * @brief Return true if the event loop is running.
     *
     * @return true
     * @return false
     */
    bool isRunning()
    {
        return looping_ && (!quit_);
    }

    /**
     * @brief Check if the event loop is calling a function.
     *
     * @return true
     * @return false
     */
    bool isCallingFunctions()
    {
        return callingFuncs_;
    }

  private:
    void abortNotInLoopThread();
    void wakeup();
    void wakeupRead();
    bool looping_;
    std::thread::id threadId_;
    bool quit_;
    std::unique_ptr<Poller> poller_;

    ChannelList activeChannels_;
    Channel *currentActiveChannel_;

    bool eventHandling_;
    MpscQueue<Func> funcs_;
    std::unique_ptr<TimerQueue> timerQueue_;
    bool callingFuncs_{false};
#ifdef __linux__
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannelPtr_;
#elif defined _WIN32
#else
    int wakeupFd_[2];
    std::unique_ptr<Channel> wakeupChannelPtr_;
#endif

    void doRunInLoopFuncs();
#ifdef _WIN32
    size_t index_{size_t(-1)};
#else
    size_t index_{std::numeric_limits<size_t>::max()};
#endif
    EventLoop **threadLocalLoopPtr_;
};

}  // namespace trantor
