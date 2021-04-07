/**
 *
 *  @file NonCopyable.h
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

#include <trantor/exports.h>

namespace trantor
{
/**
 * @brief This class represents a non-copyable object.
 *
 */
class TRANTOR_EXPORT NonCopyable
{
  protected:
    NonCopyable()
    {
    }
    ~NonCopyable()
    {
    }
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;
    // some uncopyable classes maybe support move constructor....
    NonCopyable(NonCopyable &&) noexcept(true) = default;
    NonCopyable &operator=(NonCopyable &&) noexcept(true) = default;
};

}  // namespace trantor
