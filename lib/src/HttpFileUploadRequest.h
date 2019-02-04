/**
 *
 *  HttpFileUploadRequest.h
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
#include "HttpRequestImpl.h"
#include <string>
#include <vector>

namespace drogon
{

class HttpFileUploadRequest : public HttpRequestImpl
{
  public:
    const std::string &boundary() const { return _boundary; }
    const std::vector<UploadFile> &files() const { return _files; }
    explicit HttpFileUploadRequest(const std::vector<UploadFile> &files);

  private:
    std::string _boundary;
    std::vector<UploadFile> _files;
};

} // namespace drogon