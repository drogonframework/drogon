/**
 *
 *  @file PgListener.h
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

#include <drogon/orm/DbListener.h>
#include <trantor/net/EventLoopThread.h>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include "./PgConnection.h"

namespace drogon
{
namespace orm
{
class PgListener : public DbListener,
                   public std::enable_shared_from_this<PgListener>
{
  public:
    PgListener(std::string connInfo, trantor::EventLoop *loop);
    ~PgListener() override;
    void init() noexcept;

    trantor::EventLoop *loop() const
    {
        return loop_;
    }

    void listen(const std::string &channel,
                MessageCallback messageCallback) noexcept override;
    void unlisten(const std::string &channel) noexcept override;

    // methods below should be called in loop

    void onMessage(const std::string &channel,
                   const std::string &message) const noexcept;
    void listenAll() noexcept;
    void listenNext() noexcept;

  private:
    /// Escapes a string for use as an SQL identifier, such as a table, column,
    /// or function name. This is useful when a user-supplied identifier might
    /// contain special characters that would otherwise not be interpreted as
    /// part of the identifier by the SQL parser, or when the identifier might
    /// contain upper case characters whose case should be preserved.
    /**
     * @param str: c-style string to escape. A terminating zero byte is not
     * required, and should not be counted in length(If a terminating zero byte
     * is found before length bytes are processed, PQescapeIdentifier stops at
     * the zero; the behavior is thus rather like strncpy).
     * @param length: length of the c-style string
     * @return:  The return string has all special characters replaced so that
     * it will be properly processed as an SQL identifier. A terminating zero
     * byte is also added. The return string will also be surrounded by double
     * quotes.
     */
    static std::string escapeIdentifier(const PgConnectionPtr &conn,
                                        const char *str,
                                        size_t length);

    void listenInLoop(const std::string &channel,
                      bool listen,
                      std::shared_ptr<unsigned int> = nullptr);

    PgConnectionPtr newConnection(std::shared_ptr<unsigned int> = nullptr);

    std::string connectionInfo_;
    std::unique_ptr<trantor::EventLoopThread> threadPtr_;
    trantor::EventLoop *loop_;
    DbConnectionPtr connHolder_;
    DbConnectionPtr conn_;
    std::deque<std::pair<bool, std::string>> listenTasks_;
    std::unordered_map<std::string, std::vector<MessageCallback>>
        listenChannels_;
};

}  // namespace orm
}  // namespace drogon
