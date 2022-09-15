/**
 *
 *  @file DbSubscriber.h
 *  @author Nitromelon
 *
 *  Copyright 2022, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/exports.h>
#include <memory>
#include <functional>

namespace drogon
{
namespace orm
{
class DROGON_EXPORT DbSubscriber
{
  public:
    using MessageCallback = std::function<void(const std::string &channel,
                                               const std::string &payload)>;

    virtual ~DbSubscriber() = default;

    virtual void subscribe(const std::string &channel,
                           MessageCallback &&messageCallback) noexcept = 0;
    virtual void unsubscribe(const std::string &channel) noexcept = 0;
};

}  // namespace orm
}  // namespace drogon
