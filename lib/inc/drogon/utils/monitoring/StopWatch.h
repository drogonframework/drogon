/**
 *
 *  StopWatch.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <chrono>
#include <functional>
#include <assert.h>

namespace drogon
{
/**
 * @brief This class is used to measure the elapsed time.
 */
class StopWatch
{
  public:
    StopWatch() : start_(std::chrono::steady_clock::now())
    {
    }

    ~StopWatch()
    {
    }

    /**
     * @brief Reset the start time.
     */
    void reset()
    {
        start_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Get the elapsed time in seconds.
     */
    double elapsed() const
    {
        return std::chrono::duration_cast<std::chrono::duration<double>>(
                   std::chrono::steady_clock::now() - start_)
            .count();
    }

  private:
    std::chrono::steady_clock::time_point start_;
};

class LifeTimeWatch
{
  public:
    LifeTimeWatch(std::function<void(double)> callback)
        : stopWatch_(), callback_(std::move(callback))
    {
        assert(callback_);
    }

    ~LifeTimeWatch()
    {
        callback_(stopWatch_.elapsed());
    }

  private:
    StopWatch stopWatch_;
    std::function<void(double)> callback_;
};
}  // namespace drogon
