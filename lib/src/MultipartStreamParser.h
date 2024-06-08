/**
 *
 *  @file MultipartStreamParser.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once
#include <string>
#include <variant>
#include <deque>

#include <trantor/utils/MsgBuffer.h>
#include <drogon/HttpStreamHandler.h>  // TODO

namespace drogon
{
class MultipartStreamParser
{
  public:
    MultipartStreamParser(const std::string &contentType);

    void parse(const char *data,
               size_t length,
               const HttpStreamHandler::MultipartHeaderCallback &headerCb,
               const HttpStreamHandler::StreamDataCallback &dataCb);

    bool isFinished() const
    {
        return isFinished_;
    }

    bool isValid() const
    {
        return isValid_;
    }

  private:
    const std::string dash_ = "--";
    const std::string crlf_ = "\r\n";
    std::string boundary_;
    std::string dashBoundaryCrlf_;
    std::string crlfDashBoundary_;

    // TODO: MsgBuffer is not suitable here. The underlying vector will keep
    // growing.
    trantor::MsgBuffer buffer_;

    enum class Status
    {
        kExpectFirstBoundary = 0,
        kExpectNewEntry = 1,
        kExpectHeader = 2,
        kExpectBody = 3,
        kExpectEndOrNewEntry = 4,
    } status_{Status::kExpectFirstBoundary};

    MultipartHeader currentHeader_;
    bool isValid_{true};
    bool isFinished_{false};
};
}  // namespace drogon
