/**
 *
 *  HttpResponseParser.h
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

#include "impl_forwards.h"
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/TcpConnection.h>
#include <trantor/utils/MsgBuffer.h>
#include <list>
#include <mutex>

namespace drogon
{
class HttpResponseParser : public trantor::NonCopyable
{
  public:
    enum class HttpResponseParseState
    {
        kExpectResponseLine,
        kExpectHeaders,
        kExpectBody,
        kExpectChunkLen,
        kExpectChunkBody,
        kExpectLastEmptyChunk,
        kExpectClose,
        kGotAll,
    };

    HttpResponseParser();

    // default copy-ctor, dtor and assignment are fine

    // return false if any error
    bool parseResponse(trantor::MsgBuffer *buf);

    bool gotAll() const
    {
        return _state == HttpResponseParseState::kGotAll;
    }

    void setForHeadMethod()
    {
        _parseResponseForHeadMethod = true;
    }

    void reset();

    const HttpResponseImplPtr &responseImpl() const
    {
        return _response;
    }

  private:
    bool processResponseLine(const char *begin, const char *end);

    HttpResponseParseState _state;
    HttpResponseImplPtr _response;
    bool _parseResponseForHeadMethod = false;
};

}  // namespace drogon
