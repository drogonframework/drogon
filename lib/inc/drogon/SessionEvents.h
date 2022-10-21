/**
 *
 *  @file SessionEvents.h
 *  @author An Tao
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

class SessionEvents 
{
public:
  virtual void onSessionStart( const std::string& sessionId ) noexcept = 0;
  virtual void onSessionDestroy( const std::string& sessionId ) noexcept = 0;
};

using SessionEventsPtr = std::unique_ptr<SessionEvents>;

}  // namespace drogon
