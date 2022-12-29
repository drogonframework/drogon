/**
 *
 *  @file DbListener.h
 *  @author Nitromelon
 *
 *  Copyright 2022, An Tao.  All rights reserved.
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
class DbClient;

/// Database asynchronous notification listener abstract class
class DROGON_EXPORT DbListener
{
  public:
    virtual ~DbListener();

    /// Create a new notification listener upon a DbClient
    /**
     * @param dbClient: DbClient instance the listener relies on
     *
     * @note
     * Currently only postgresql supports asynchronous notification
     *
     */
    static std::shared_ptr<DbListener> newDbListener(
        std::shared_ptr<DbClient> dbClient);

    /// Get the underlying DbClient
    virtual std::shared_ptr<DbClient> dbClient() const = 0;

    /// Listen to a channel
    /**
     * @param channel channel name to listen
     * @param messageCallback callback when notification arrives on channel
     */
    virtual void listen(
        const std::string &channel,
        std::function<void(std::string, std::string)> messageCallback) = 0;

    virtual void unlisten(const std::string &channel) noexcept;
};

}  // namespace orm
}  // namespace drogon
