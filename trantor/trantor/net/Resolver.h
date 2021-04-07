// Copyright 2016, Tao An.  All rights reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Tao An

#pragma once
#include <trantor/exports.h>
#include <memory>
#include <trantor/net/EventLoop.h>
#include <trantor/net/InetAddress.h>

namespace trantor
{
/**
 * @brief This class represents an asynchronous DNS resolver.
 * @note Although the c-ares library is not essential, it is recommended to
 * install it for higher performance
 */
class TRANTOR_EXPORT Resolver
{
  public:
    using Callback = std::function<void(const trantor::InetAddress&)>;

    /**
     * @brief Create a new DNS resolver.
     *
     * @param loop The event loop in which the DNS resolver runs.
     * @param timeout The timeout in seconds for DNS.
     * @return std::shared_ptr<Resolver>
     */
    static std::shared_ptr<Resolver> newResolver(EventLoop* loop = nullptr,
                                                 size_t timeout = 60);

    /**
     * @brief Resolve an address asynchronously.
     *
     * @param hostname
     * @param callback
     */
    virtual void resolve(const std::string& hostname,
                         const Callback& callback) = 0;

    virtual ~Resolver()
    {
    }

    /**
     * @brief Check whether the c-ares library is used.
     *
     * @return true
     * @return false
     */
    static bool isCAresUsed();
};
}  // namespace trantor