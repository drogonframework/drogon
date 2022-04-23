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
#include <drogon/utils/string_view.h>
#include "filesystem.h"
#include <drogon/HttpRequest.h>

#include <map>
#include <string>
#include <vector>
#include <memory>

namespace drogon
{
class HttpFileImpl
{
  public:
    /// Return the file name;
    const std::string &getFileName() const
    {
        return fileName_;
    }

    /// Set the file name, usually called by the MultiPartParser parser.
    void setFileName(const std::string &fileName)
    {
        fileName_ = fileName;
    }

    /// Return the file extension;
    string_view getFileExtension() const
    {
        return drogon::getFileExtension(fileName_);
    }

    /// Set the contents of the file, usually called by the MultiPartParser
    /// parser.
    void setFile(const char *data, size_t length)
    {
        fileContent_ = string_view{data, length};
    }

    /// Save the file to the file system.
    /**
     * The folder saving the file is app().getUploadPath().
     * The full path is app().getUploadPath()+"/"+this->getFileName()
     */
    int save() const;

    /// Save the file to @param path
    /**
     * @param path if the parameter is prefixed with "/", "./" or "../", or is
     * "." or "..", the full path is path+"/"+this->getFileName(),
     * otherwise the file is saved as
     * app().getUploadPath()+"/"+path+"/"+this->getFileName()
     */
    int save(const std::string &path) const;

    /// Save the file to file system with a new name
    /**
     * @param fileName if the parameter isn't prefixed with "/", "./" or "../",
     * the full path is app().getUploadPath()+"/"+filename, otherwise the file
     * is saved as the filename
     */
    int saveAs(const std::string &fileName) const;

    /// Return the file length.
    size_t fileLength() const noexcept
    {
        return fileContent_.length();
    }

    const char *fileData() const noexcept
    {
        return fileContent_.data();
    }

    const string_view &fileContent() const noexcept
    {
        return fileContent_;
    }

    /// Return the name of the item in multiple parts.
    const std::string &getItemName() const
    {
        return itemName_;
    }

    void setItemName(const std::string &itemName)
    {
        itemName_ = itemName;
    }

    /// Return the type of file.
    FileType getFileType() const
    {
        return parseFileType(getFileExtension());
    }

    /// Return the md5 string of the file
    std::string getMd5() const;
    //    int saveTo(const std::string &pathAndFileName) const;
    int saveTo(const filesystem::path &pathAndFileName) const;
    void setRequest(const HttpRequestPtr &req)
    {
        requestPtr_ = req;
    }

  private:
    std::string fileName_;
    std::string itemName_;
    string_view fileContent_;
    HttpRequestPtr requestPtr_;
};
}  // namespace drogon
