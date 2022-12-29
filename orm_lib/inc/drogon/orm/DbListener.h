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
#include <functional>
#include <string>
#include <memory>

namespace drogon
{
namespace orm
{
class DbClient;
class DbListener;
using DbListenerPtr = std::shared_ptr<DbListener>;

/// Database asynchronous notification listener abstract class
class DROGON_EXPORT DbListener
{
  public:
    using MessageCallback = std::function<void(std::string, std::string)>;

    explicit DbListener(std::shared_ptr<DbClient> dbClient);
    virtual ~DbListener();

    /// Create a new notification listener upon a DbClient
    /**
     * @param dbClient: DbClient instance the listener relies on
     *
     * @note
     * Currently only postgresql supports asynchronous notification
     *
     */
    static DbListenerPtr newDbListener(std::shared_ptr<DbClient> dbClient);
    static DbListenerPtr getDbClientListener(
        const std::shared_ptr<DbClient> &dbClient);

    /// Get the underlying DbClient
    std::shared_ptr<DbClient> dbClient() const
    {
        return dbClient_;
    }

    /// Listen to a channel
    /**
     * @param channel channel name to listen
     * @param messageCallback callback when notification arrives on channel
     */
    virtual void listen(const std::string &channel,
                        MessageCallback messageCallback) = 0;

    virtual void unlisten(const std::string &channel) noexcept = 0;

  protected:
    std::shared_ptr<DbClient> dbClient_;
};

}  // namespace orm
}  // namespace drogon
