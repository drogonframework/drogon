#pragma once

#include "PgConnection.h"
#include "../DbClientGeneralImpl.h"
#include <trantor/net/EventLoop.h>
#include <memory>
#include <thread>
#include <functional>
#include <string>
#include <unordered_set>
#include <list>

namespace drogon
{
namespace orm
{

class PgClientImpl : public DbClientGeneralImpl, public std::enable_shared_from_this<PgClientImpl>
{
public:
  PgClientImpl(const std::string &connInfo, const size_t connNum) : DbClientGeneralImpl(connInfo, connNum) {}

private:
  virtual DbConnectionPtr newConnection() override;
};

} // namespace orm

} // namespace drogon
