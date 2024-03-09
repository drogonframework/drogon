/**
 *
 *  Registry.h
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
#include <memory>

namespace drogon
{
namespace monitoring
{
class CollectorBase;

/**
 * This class is used to register metrics.
 * */
class Registry
{
  public:
    virtual ~Registry() = default;
    virtual void registerCollector(
        const std::shared_ptr<CollectorBase> &collector) = 0;
};
}  // namespace monitoring
}  // namespace drogon
