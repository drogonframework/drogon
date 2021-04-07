/**
 *
 *  @file TimingWheel.h
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

#include <trantor/net/EventLoop.h>
#include <trantor/utils/Logger.h>
#include <trantor/exports.h>
#include <map>
#include <mutex>
#include <deque>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <assert.h>

#define TIMING_BUCKET_NUM_PER_WHEEL 100
#define TIMING_TICK_INTERVAL 1.0

namespace trantor
{
using EntryPtr = std::shared_ptr<void>;

using EntryBucket = std::unordered_set<EntryPtr>;
using BucketQueue = std::deque<EntryBucket>;

/**
 * @brief This class implements a timer strategy with high performance and low
 * accuracy. This is usually used internally.
 *
 */
class TRANTOR_EXPORT TimingWheel
{
  public:
    class CallbackEntry
    {
      public:
        CallbackEntry(std::function<void()> cb) : cb_(std::move(cb))
        {
        }
        ~CallbackEntry()
        {
            cb_();
        }

      private:
        std::function<void()> cb_;
    };

    /**
     * @brief Construct a new timing wheel instance.
     *
     * @param loop The event loop in which the timing wheel runs.
     * @param maxTimeout The maximum timeout of the timing wheel.
     * @param ticksInterval The internal timer tick interval.  It affects the
     * accuracy of the timing wheel.
     * @param bucketsNumPerWheel The number of buckets per wheel.
     * @note The max delay of the timing wheel is about
     * ticksInterval*(bucketsNumPerWheel^wheelsNum) seconds.
     * @example Four wheels with 200 buckets per wheel means the timing wheel
     * can work with a timeout up to 200^4 seconds, about 50 years;
     */
    TimingWheel(trantor::EventLoop *loop,
                size_t maxTimeout,
                float ticksInterval = TIMING_TICK_INTERVAL,
                size_t bucketsNumPerWheel = TIMING_BUCKET_NUM_PER_WHEEL);

    void insertEntry(size_t delay, EntryPtr entryPtr);

    void insertEntryInloop(size_t delay, EntryPtr entryPtr);

    EventLoop *getLoop()
    {
        return loop_;
    }

    ~TimingWheel();

  private:
    std::vector<BucketQueue> wheels_;

    std::atomic<size_t> ticksCounter_{0};

    trantor::TimerId timerId_;
    trantor::EventLoop *loop_;

    float ticksInterval_;
    size_t wheelsNum_;
    size_t bucketsNumPerWheel_;
};
}  // namespace trantor
