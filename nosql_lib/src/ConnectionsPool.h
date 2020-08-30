/**
 *
 *  @file ConnectionsPool.h
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
#include <functional>
#include <unordered_set>
#include <deque>
namespace drogon
{
namespace nosql
{
template <typename ConnType, typename CommandType>
class ConnectionsPool
{
  public:
    using ConnectionGenerator = std::function<ConnType()>;
    ConnectionsPool() = default;
    void addNewConnection(ConnType &&conn)
    {
        connections_.insert(std::move(conn));
    }
    std::pair<bool, ConnType> getIdleConnection()
    {
        if (readyConnections_.size() == 0)
        {
            if (commandsBuffer_.size() > 200000)
            {
                // too many queries in buffer;
                return {false, nullptr};
            }
            else
            {
                return {true, nullptr};
            }
        }
        else
        {
            auto iter = readyConnections_.begin();
            busyConnections_.insert(*iter);
            ConnType conn = std::move(*iter);
            readyConnections_.erase(iter);
            return {true, std::move(conn)};
        }
    }
    void putCommandToBuffer(CommandType &&command)
    {
        commandsBuffer_.emplace_back(std::move(command));
    }
    CommandType getCommand()
    {
        assert(!commandsBuffer_.empty());
        auto command = std::move(commandsBuffer_.front());
        commandsBuffer_.pop_front();
        return command;
    }
    bool hasCommand()
    {
        return !commandsBuffer_.empty();
    }
    void addBusyConnection(const ConnType &conn)
    {
        busyConnections_.insert(conn);
    }
    void setConnectionIdle(const ConnType &conn)
    {
        busyConnections_.erase(conn);
        readyConnections_.insert(conn);
    }
    void removeConnection(const ConnType &conn)
    {
        readyConnections_.erase(conn);
        busyConnections_.erase(conn);
        assert(connections_.find(conn) != connections_.end());
        connections_.erase(conn);
    }

  private:
    std::unordered_set<ConnType> connections_;
    std::unordered_set<ConnType> readyConnections_;
    std::unordered_set<ConnType> busyConnections_;
    std::deque<CommandType> commandsBuffer_;
};
}  // namespace nosql
}  // namespace drogon