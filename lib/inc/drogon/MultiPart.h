/**
 *
 *  MultiPart.h
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

#include <map>
#include <string>
#include <vector>

namespace drogon
{
class HttpFile
{
  public:
    /// Return the file name;
    const std::string &getFileName() const
    {
        return _fileName;
    };

    /// Set the file name
    void setFileName(const std::string &filename)
    {
        _fileName = filename;
    };

    /// Set the contents of the file, usually called by the FileUpload parser.
    void setFile(const std::string &file)
    {
        _fileContent = file;
    };
    void setFile(std::string &&file)
    {
        _fileContent = std::move(file);
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
     * @param filename if the parameter isn't prefixed with "/", "./" or "../",
     * the full path is app().getUploadPath()+"/"+filename, otherwise the file
     * is saved as the filename
     */
    int saveAs(const std::string &filename) const;

    /// Return the file length.
    int64_t fileLength() const
    {
        return _fileContent.length();
    };

    /// Return the md5 string of the file
    const std::string getMd5() const;

  protected:
    int saveTo(const std::string &pathAndFilename) const;
    std::string _fileName;
    std::string _fileContent;
};

/// A parser class which help the user to get the files and the parameters in
/// the multipart format request.
class MultiPartParser
{
  public:
    MultiPartParser(){};
    ~MultiPartParser(){};
    /// Get files, This method should be called after calling the parse()
    /// method.
    const std::vector<HttpFile> &getFiles();

    /// Get parameters, This method should be called after calling the parse ()
    /// method.
    const std::map<std::string, std::string> &getParameters() const;

    /// Parse the http request stream to get files and parameters.
    int parse(const HttpRequestPtr &req);

  protected:
    std::vector<HttpFile> _files;
    std::map<std::string, std::string> _parameters;
    int parse(const HttpRequestPtr &req, const std::string &boundary);
    int parseEntity(const char *begin, const char *end);
};

/// In order to be compatible with old interfaces
typedef MultiPartParser FileUpload;

}  // namespace drogon
