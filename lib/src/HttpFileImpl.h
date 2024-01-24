/**
 *
 *  @file HttpFileImpl.h
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
#include "HttpUtils.h"
#include <drogon/HttpRequest.h>

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <string_view>

namespace drogon
{
class HttpFileImpl
{
  public:
    /// Return the file name;
    const std::string &getFileName() const noexcept
    {
        return fileName_;
    }

    /// Set the file name, usually called by the MultiPartParser parser.
    void setFileName(const std::string &fileName) noexcept
    {
        fileName_ = fileName;
    }

    void setFileName(std::string &&fileName) noexcept
    {
        fileName_ = std::move(fileName);
    }

    /// Return the file extension;
    std::string_view getFileExtension() const noexcept
    {
        return drogon::getFileExtension(fileName_);
    }

    /// Set the contents of the file, usually called by the MultiPartParser
    /// parser.
    void setFile(const char *data, size_t length) noexcept
    {
        fileContent_ = std::string_view{data, length};
    }

    /// Save the file to the file system.
    /**
     * The folder saving the file is app().getUploadPath().
     * The full path is app().getUploadPath()+"/"+this->getFileName()
     */
    int save() const noexcept;

    /// Save the file to @param path
    /**
     * @param path if the parameter is prefixed with "/", "./" or "../", or is
     * "." or "..", the full path is path+"/"+this->getFileName(),
     * otherwise the file is saved as
     * app().getUploadPath()+"/"+path+"/"+this->getFileName()
     */
    int save(const std::string &path) const noexcept;

    /// Save the file to file system with a new name
    /**
     * @param fileName if the parameter isn't prefixed with "/", "./" or "../",
     * the full path is app().getUploadPath()+"/"+filename, otherwise the file
     * is saved as the filename
     */
    int saveAs(const std::string &fileName) const noexcept;

    /// Return the file length.
    size_t fileLength() const noexcept
    {
        return fileContent_.length();
    }

    const char *fileData() const noexcept
    {
        return fileContent_.data();
    }

    const std::string_view &fileContent() const noexcept
    {
        return fileContent_;
    }

    /// Return the name of the item in multiple parts.
    const std::string &getItemName() const noexcept
    {
        return itemName_;
    }

    void setItemName(const std::string &itemName) noexcept
    {
        itemName_ = itemName;
    }

    void setItemName(std::string &&itemName) noexcept
    {
        itemName_ = std::move(itemName);
    }

    /// Return the type of file.
    FileType getFileType() const noexcept
    {
        auto ft = drogon::getFileType(contentType_);
        if ((ft != FT_UNKNOWN) && (ft != FT_CUSTOM))
            return ft;
        return parseFileType(getFileExtension());
    }

    /// Return md5 hash of the file
    std::string getMd5() const noexcept;
    // Return sha1 hash of the file
    std::string getSha256() const noexcept;
    // Return sha512 hash of the file
    std::string getSha3() const noexcept;
    //    int saveTo(const std::string &pathAndFileName) const;
    int saveTo(const std::filesystem::path &pathAndFileName) const noexcept;

    void setRequest(const HttpRequestPtr &req) noexcept
    {
        requestPtr_ = req;
    }

    drogon::ContentType getContentType() const noexcept
    {
        return contentType_;
    }

    void setContentType(drogon::ContentType contentType) noexcept
    {
        contentType_ = contentType;
    }

    void setContentTransferEncoding(
        const std::string &contentTransferEncoding) noexcept
    {
        transferEncoding_ = contentTransferEncoding;
    }

    void setContentTransferEncoding(
        std::string &&contentTransferEncoding) noexcept
    {
        transferEncoding_ = std::move(contentTransferEncoding);
    }

    const std::string &getContentTransferEncoding() const noexcept
    {
        return transferEncoding_;
    }

  private:
    std::string fileName_;
    std::string itemName_;
    std::string transferEncoding_;
    std::string_view fileContent_;
    HttpRequestPtr requestPtr_;
    drogon::ContentType contentType_{drogon::CT_NONE};
};
}  // namespace drogon
