/**
 *
 *  HttpFileUploadRequest.cc
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

#include "HttpFileUploadRequest.h"
#include <drogon/utils/Utilities.h>

using namespace drogon;

HttpFileUploadRequest::HttpFileUploadRequest(const std::vector<UploadFile> &files)
    : HttpRequestImpl(nullptr),
      _boundary(genRandomString(32)),
      _files(files)
{
    setMethod(drogon::Post);
    setVersion(drogon::HttpRequest::kHttp11);
    setContentType("Content-Type: multipart/form-data; boundary=" + _boundary + "\r\n");
    _contentType = CT_MULTIPART_FORM_DATA;
}