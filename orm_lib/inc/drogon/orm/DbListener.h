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

namespace trantor
{
class EventLoop;
}

namespace drogon
{
namespace orm
{
class DbListener;
using DbListenerPtr = std::shared_ptr<DbListener>;

/// Database asynchronous notification listener abstract class
class DROGON_EXPORT DbListener
{
  public:
    using MessageCallback = std::function<void(std::string, std::string)>;

    virtual ~DbListener();

    /// Create a new postgresql notification listener
    /**
     * @param connInfo: Connection string, the same as DbClient::newPgClient()
     * @param loop: The eventloop this DbListener runs in. If empty, a new
     * thread will be created.
     * @return DbListenerPtr
     * @return nullptr if postgresql is not supported.
     */
    static DbListenerPtr newPgListener(const std::string &connInfo,
                                       trantor::EventLoop *loop = nullptr);

    /// Listen to a channel
    /**
     * @param channel channel name to listen
     * @param messageCallback callback when notification arrives on channel
     *
     * @note `listen()` can be called on the same channel multiple times.
     * In this case, each `messageCallback` will be called when message arrives.
     * However, a single `unlisten()` call will cancel all the callbacks.
     *
     * @note If has connection issues, the listener will keep retrying until
     * listen success. The listener will also re-listen to all channels after
     * re-connection.
     * However, if user passes an invalid channel string, the operation will
     * fail with an error log without any other actions. (This behavior may
     * change in future. A errorCallback may be added as a parameters.)
     */
    virtual void listen(const std::string &channel,
                        MessageCallback messageCallback) noexcept = 0;

    /// Stop listening to channel
    /**
     * @param channel channel to stop listening
     */
    virtual void unlisten(const std::string &channel) noexcept = 0;
};

}  // namespace orm
}  // namespace drogon
