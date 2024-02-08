/**
 *  SpinLock.h
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
#include <atomic>
#include <emmintrin.h>
#include <thread>

#define LOCK_SPIN 2048

namespace drogon
{
class SpinLock
{
  public:
    inline SpinLock(std::atomic<bool> &flag) : flag_(flag)
    {
        static const int cpu = std::thread::hardware_concurrency();
        int n, i;
        while (1)
        {
            if (!flag_.load() &&
                !flag_.exchange(true, std::memory_order_acquire))
            {
                return;
            }
            if (cpu > 1)
            {
                for (n = 1; n < LOCK_SPIN; n <<= 1)
                {
                    for (i = 0; i < n; ++i)
                    {
                        //__asm__ __volatile__("rep; nop" ::: "memory"); //pause
                        _mm_pause();
                    }
                    if (!flag_.load() &&
                        !flag_.exchange(true, std::memory_order_acquire))
                    {
                        return;
                    }
                }
            }
            std::this_thread::yield();
        }
    }

    inline ~SpinLock()
    {
        flag_.store(false, std::memory_order_release);
    }

  private:
    std::atomic<bool> &flag_;
};

class SimpleSpinLock
{
  public:
    inline SimpleSpinLock(std::atomic_flag &flag) : flag_(flag)
    {
        while (flag_.test_and_set(std::memory_order_acquire))
        {
            //__asm__ __volatile__("rep; nop" ::: "memory"); //pause
            _mm_pause();
        }
    }

    inline ~SimpleSpinLock()
    {
        flag_.clear(std::memory_order_release);
    }

  private:
    std::atomic_flag &flag_;
};

}  // namespace drogon
