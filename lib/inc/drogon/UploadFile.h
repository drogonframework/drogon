/**
 *
 *  UploadFile.h
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
#include <string>

namespace drogon
{
/**
 * This class represents an upload file which will be transferred to the server
 * via the multipart/form-data format
 */
class UploadFile
{
  public:
    /// Constructor
    /**
     * @param filePath The file location on local host, including file name.
     * @param fileName The file name provided to the server. If it is empty by
     * default, the file name in the @param filePath
     * is provided to the server.
     * @param itemName The item name on the browser form.
     */
    explicit UploadFile(const std::string &filePath,
                        const std::string &fileName = "",
                        const std::string &itemName = "file")
        : path_(filePath), itemName_(itemName)
    {
        if (!fileName.empty())
        {
            fileName_ = fileName;
        }
        else
        {
            auto pos = filePath.rfind('/');
            if (pos != std::string::npos)
            {
                fileName_ = filePath.substr(pos + 1);
            }
            else
            {
                fileName_ = filePath;
            }
        }
    }
    const std::string &path() const
    {
        return path_;
    }
    const std::string &fileName() const
    {
        return fileName_;
    }
    const std::string &itemName() const
    {
        return itemName_;
    }

  private:
    std::string path_;
    std::string fileName_;
    std::string itemName_;
};
}  // namespace drogon
