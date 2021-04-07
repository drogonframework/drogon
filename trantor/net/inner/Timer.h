/**
 *
 *  Timer.h
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
#include <trantor/net/callbacks.h>
#include <functional>
#include <atomic>
#include <iostream>
#include <chrono>

namespace trantor
{
using TimerId = uint64_t;
using TimePoint = std::chrono::steady_clock::time_point;
using TimeInterval = std::chrono::microseconds;
class Timer : public NonCopyable
{
  public:
    Timer(const TimerCallback &cb,
          const TimePoint &when,
          const TimeInterval &interval);
    Timer(TimerCallback &&cb,
          const TimePoint &when,
          const TimeInterval &interval);
    ~Timer()
    {
        //   std::cout<<"Timer unconstract!"<<std::endl;
    }
    void run() const;
    void restart(const TimePoint &now);
    bool operator<(const Timer &t) const;
    bool operator>(const Timer &t) const;
    const TimePoint &when() const
    {
        return when_;
    }
    bool isRepeat()
    {
        return repeat_;
    }
    TimerId id()
    {
        return id_;
    }

  private:
    TimerCallback callback_;
    TimePoint when_;
    const TimeInterval interval_;
    const bool repeat_;
    const TimerId id_;
    static std::atomic<TimerId> timersCreated_;
};

}  // namespace trantor
