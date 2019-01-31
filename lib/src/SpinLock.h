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
#include <thread>

namespace drogon
{

class SpinLock
{
  public:
    SpinLock(std::atomic<bool> &flag);
    ~SpinLock();

  private:
    std::atomic<bool> &_flag;
};

class SimpleSpinLock
{
  public:
    SimpleSpinLock(std::atomic_flag &flag);
    ~SimpleSpinLock();

  private:
    std::atomic_flag &_flag;
};


} // namespace drogon