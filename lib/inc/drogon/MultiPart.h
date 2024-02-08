/**
 *
 *  @file MultiPart.h
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

#include "drogon/utils/Utilities.h"
#include <drogon/exports.h>
#include <drogon/HttpRequest.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <string_view>

namespace drogon
{
class HttpFileImpl;

/**
 * @brief This class represents a uploaded file by a HTTP request.
 *
 */
class DROGON_EXPORT HttpFile
{
  public:
    explicit HttpFile(std::shared_ptr<HttpFileImpl> &&implPtr) noexcept;
    /// Return the file name;
    const std::string &getFileName() const noexcept;

    /// Return the file extension;
    /// Note: After the HttpFile object is destroyed, do not use this
    /// std::string_view object.
    std::string_view getFileExtension() const noexcept;

    /// Return the name of the item in multiple parts.
    const std::string &getItemName() const noexcept;

    /// Return the type of file.
    FileType getFileType() const noexcept;

    /// Set the file name, usually called by the MultiPartParser parser.
    void setFileName(const std::string &fileName) noexcept;

    /// Set the contents of the file, usually called by the MultiPartParser
    /// parser.
    void setFile(const char *data, size_t length) noexcept;

    /// Save the file to the file system.
    /**
     * The folder saving the file is app().getUploadPath().
     * The full path is app().getUploadPath()+"/"+this->getFileName()
     */
    int save() const noexcept;

    /// Save the file to @p path
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

    /**
     * @brief return the content of the file.
     *
     * @return std::string_view
     */
    std::string_view fileContent() const noexcept
    {
        return std::string_view{fileData(), fileLength()};
    }

    /// Return the file length.
    size_t fileLength() const noexcept;

    /// Return the content-type of the file.
    drogon::ContentType getContentType() const noexcept;
    /**
     * @brief return the pointer of the file data.
     *
     * @return const char*
     * @note This function just returns the beginning of the file data in
     * memory. Users mustn't assume that there is an \0 character at the end of
     * the file data even if the type of the file is text. One should get the
     * length of the file by the fileLength() method, or use the fileContent()
     * method.
     */
    const char *fileData() const noexcept;

    /// Return the md5 string of the file
    std::string getMd5() const noexcept;

    /// Return the content transfer encoding of the file.
    const std::string &getContentTransferEncoding() const noexcept;

  private:
    std::shared_ptr<HttpFileImpl> implPtr_;
};

/// A parser class which help the user to get the files and the parameters in
/// the multipart format request.
class DROGON_EXPORT MultiPartParser
{
  public:
    MultiPartParser(){};
    ~MultiPartParser(){};
    /// Get files, This method should be called after calling the parse()
    /// method.
    const std::vector<HttpFile> &getFiles() const;

    /// Get files in a map, the keys of the map are item names of the files.
    std::unordered_map<std::string, HttpFile> getFilesMap() const;

    /// Get parameters, This method should be called after calling the parse ()
    /// method.
    const SafeStringMap<std::string> &getParameters() const;

    /// Get the value of an optional parameter
    /// This method should be called after calling the parse() method.
    template <typename T>
    std::optional<T> getOptionalParameter(const std::string &key)
    {
        auto &params = getParameters();
        auto it = params.find(key);
        if (it != params.end())
        {
            try
            {
                return std::optional<T>(utils::fromString<T>(it->second));
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << e.what();
                return std::optional<T>{};
            }
        }
        else
        {
            return std::optional<T>{};
        }
    }

    /// Get the value of a parameter
    /// This method should be called after calling the parse() method.
    /// Note: returns a default T object if the parameter is missing
    template <typename T>
    T getParameter(const std::string &key)
    {
        return getOptionalParameter<T>(key).value_or(T{});
    }

    /// Parse the http request stream to get files and parameters.
    int parse(const HttpRequestPtr &req);

  protected:
    std::vector<HttpFile> files_;
    SafeStringMap<std::string> parameters_;
    int parse(const HttpRequestPtr &req,
              const char *boundaryData,
              size_t boundaryLen);
    int parseEntity(const char *begin, const char *end);
    HttpRequestPtr requestPtr_;
};

/// In order to be compatible with old interfaces
using FileUpload = MultiPartParser;

}  // namespace drogon
