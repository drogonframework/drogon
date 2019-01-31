/**
 *  SpinLock.cc
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

#include "SpinLock.h"

#define LOCK_SPIN 2048

using namespace drogon;

SpinLock::SpinLock(std::atomic<bool> &flag)
    : _flag(flag)
{
    for (;;)
    {
        if (!_flag.load() && !_flag.exchange(true, std::memory_order_acquire))
        {
            return;
        }
        if (std::thread::hardware_concurrency() > 1)
        {
            for (int n = 1; n < LOCK_SPIN; n <<= 1)
            {
                for (int i = 0; i < n; i++)
                {
                    __asm__ __volatile__("rep; nop" ::: "memory"); //pause
                }
                if (!_flag.load() && !_flag.exchange(true, std::memory_order_acquire))
                {
                    return;
                }
            }
        }
        std::this_thread::yield();
    }
}

SpinLock::~SpinLock()
{
    _flag.store(false, std::memory_order_release);
}
