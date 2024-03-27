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
#include <drogon/UploadFile.h>
#include <drogon/utils/Utilities.h>

#include <fstream>

using namespace drogon;

HttpFileUploadRequest::HttpFileUploadRequest(
    const std::vector<UploadFile> &files)
    : HttpRequestImpl(nullptr),
      boundary_(utils::genRandomString(32)),
      files_(files)
{
    setMethod(drogon::Post);
    setContentType("multipart/form-data; boundary=" + boundary_);
    contentType_ = CT_MULTIPART_FORM_DATA;
}

template <typename T>
void renderMultipart(const HttpFileUploadRequest *mPtr, T &content)
{
    auto &boundary = mPtr->boundary();
    for (auto &param : mPtr->getParameters())
    {
        content.append("--");
        content.append(boundary);
        content.append("\r\n");
        content.append("content-disposition: form-data; name=\"");
        content.append(param.first);
        content.append("\"\r\n\r\n");
        content.append(param.second);
        content.append("\r\n");
    }
    for (auto &file : mPtr->files())
    {
        content.append("--");
        content.append(boundary);
        content.append("\r\n");
        content.append("content-disposition: form-data; name=\"");
        content.append(file.itemName());
        content.append("\"; filename=\"");
        content.append(file.fileName());
        content.append("\"");
        if (file.contentType() != CT_NONE)
        {
            content.append("\r\n");

            auto &type = contentTypeToMime(file.contentType());
            content.append("content-type: ");
            content.append(type.data(), type.length());
        }
        content.append("\r\n\r\n");
        std::ifstream infile(utils::toNativePath(file.path()),
                             std::ifstream::binary);
        if (!infile)
        {
            LOG_ERROR << file.path() << " not found";
        }
        else
        {
            std::streambuf *pbuf = infile.rdbuf();
            std::streamsize filesize = pbuf->pubseekoff(0, infile.end);
            pbuf->pubseekoff(0, infile.beg);  // rewind
            std::string str;
            str.resize(filesize);
            pbuf->sgetn(&str[0], filesize);
            content.append(std::move(str));
        }
        content.append("\r\n");
    }
    content.append("--");
    content.append(boundary);
    content.append("--");
}

void HttpFileUploadRequest::renderMultipartFormData(
    trantor::MsgBuffer &buffer) const
{
    renderMultipart(this, buffer);
}

void HttpFileUploadRequest::renderMultipartFormData(std::string &buffer) const
{
    renderMultipart(this, buffer);
}