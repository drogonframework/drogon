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
#include <drogon/exports.h>
#include <drogon/RequestStream.h>
#include <string>

namespace drogon
{
class DROGON_EXPORT MultipartStreamParser
{
  public:
    MultipartStreamParser(const std::string &contentType);

    void parse(const char *data,
               size_t length,
               const RequestStreamReader::MultipartHeaderCallback &headerCb,
               const RequestStreamReader::StreamDataCallback &dataCb);

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

    struct Buffer
    {
      public:
        std::string_view view() const;
        void append(const char *data, size_t length);
        size_t size() const;
        void eraseFront(size_t length);
        void clear();

      private:
        std::string buffer_;
        size_t bufHead_{0};
        size_t bufTail_{0};
    } buffer_;

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
