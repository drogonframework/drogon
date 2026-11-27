/**
 *
 *  @file HttpFileImpl.cc
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

#include "HttpFileImpl.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/MultiPart.h>
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

using namespace drogon;

int HttpFileImpl::save() const noexcept
{
    return save(HttpAppFrameworkImpl::instance().getUploadPath());
}

int HttpFileImpl::save(const std::string &path) const noexcept
{
    assert(!path.empty());
    if (fileName_.empty())
        return -1;
    std::filesystem::path fsUploadDir(utils::toNativePath(path));

    std::error_code ec;
    if (fsUploadDir.is_absolute())
    {  // do nothing
    }
    else if ((!fsUploadDir.has_parent_path() ||
              (fsUploadDir.begin()->string() != "." &&
               fsUploadDir.begin()->string() != "..")))
    {
        fsUploadDir = utils::toNativePath(
                          HttpAppFrameworkImpl::instance().getUploadPath()) /
                      fsUploadDir;
    }
    else
    {
        auto curPath = std::filesystem::current_path(ec);
        if (ec)
        {
            LOG_ERROR << "Failed to get current path: " << ec.message();
            return -1;
        }
        fsUploadDir = curPath / fsUploadDir;
    }

    fsUploadDir = std::filesystem::weakly_canonical(fsUploadDir, ec);
    if (ec)
    {
        LOG_ERROR << "Failed to canonicalize upload directory: "
                  << ec.message();
        return -1;
    }

    const bool directoryExists = std::filesystem::exists(fsUploadDir, ec);
    if (ec)
    {
        LOG_ERROR << "Failed to check upload directory: " << ec.message();
        return -1;
    }

    if (!directoryExists)
    {
        LOG_TRACE << "create path:" << fsUploadDir;
        std::filesystem::create_directories(fsUploadDir, ec);
        if (ec)
        {
            LOG_ERROR << "Failed to create upload directory: " << ec.message();
            return -1;
        }
    }

    std::filesystem::path fsSaveToPath = std::filesystem::weakly_canonical(
        fsUploadDir / utils::toNativePath(fileName_), ec);
    if (ec)
    {
        LOG_ERROR << "Failed to canonicalize save path: " << ec.message();
        return -1;
    }
    LOG_TRACE << "save to path:" << fsSaveToPath;
    // fsSaveToPath can be shorter than fsUploadDir (e.g. a file name of ".."),
    // so bound both ranges and require fsUploadDir to be a prefix.
    if (std::mismatch(fsUploadDir.begin(),
                      fsUploadDir.end(),
                      fsSaveToPath.begin(),
                      fsSaveToPath.end())
            .first != fsUploadDir.end())
    {
        LOG_ERROR
            << "Attempt writing outside of upload directory detected. Path: "
            << fileName_;
        return -1;
    }

    return saveTo(fsSaveToPath);
}

int HttpFileImpl::saveAs(const std::string &fileName) const noexcept
{
    assert(!fileName.empty());
    std::filesystem::path fsFileName(utils::toNativePath(fileName));
    if (!fsFileName.is_absolute() && (!fsFileName.has_parent_path() ||
                                      (fsFileName.begin()->string() != "." &&
                                       fsFileName.begin()->string() != "..")))
    {
        std::filesystem::path fsUploadPath(utils::toNativePath(
            HttpAppFrameworkImpl::instance().getUploadPath()));
        fsFileName = fsUploadPath / fsFileName;
    }
    if (fsFileName.has_parent_path())
    {
        std::error_code ec;
        const bool parentExists =
            std::filesystem::exists(fsFileName.parent_path(), ec);
        if (ec)
        {
            LOG_ERROR << "Failed to check parent directory: " << ec.message();
            return -1;
        }
        if (!parentExists)
        {
            LOG_TRACE << "create path:" << fsFileName.parent_path();
            std::filesystem::create_directories(fsFileName.parent_path(), ec);
            if (ec)
            {
                LOG_ERROR << "Failed to create directory: " << ec.message();
                return -1;
            }
        }
    }
    return saveTo(fsFileName);
}

int HttpFileImpl::saveTo(
    const std::filesystem::path &pathAndFileName) const noexcept
{
    LOG_TRACE << "save uploaded file:" << pathAndFileName;
    auto wPath = utils::toNativePath(pathAndFileName.native());
    std::ofstream file(wPath, std::ios::binary);
    if (file.is_open())
    {
        file.write(fileContent_.data(), fileContent_.size());
        file.close();
        return 0;
    }
    else
    {
        LOG_ERROR << "save failed!";
        return -1;
    }
}

std::string HttpFileImpl::getMd5() const noexcept
{
    return utils::getMd5(fileContent_.data(), fileContent_.size());
}

std::string HttpFileImpl::getSha256() const noexcept
{
    return utils::getSha256(fileContent_.data(), fileContent_.size());
}

std::string HttpFileImpl::getSha3() const noexcept
{
    return utils::getSha3(fileContent_.data(), fileContent_.size());
}

const std::string &HttpFile::getFileName() const noexcept
{
    return implPtr_->getFileName();
}

void HttpFile::setFileName(const std::string &fileName) noexcept
{
    implPtr_->setFileName(fileName);
}

std::string_view HttpFile::getFileExtension() const noexcept
{
    return implPtr_->getFileExtension();
}

FileType HttpFile::getFileType() const noexcept
{
    return implPtr_->getFileType();
}

void HttpFile::setFile(const char *data, size_t length) noexcept
{
    implPtr_->setFile(data, length);
}

int HttpFile::save() const noexcept
{
    return implPtr_->save();
}

int HttpFile::save(const std::string &path) const noexcept
{
    return implPtr_->save(path);
}

int HttpFile::saveAs(const std::string &fileName) const noexcept
{
    return implPtr_->saveAs(fileName);
}

size_t HttpFile::fileLength() const noexcept
{
    return implPtr_->fileLength();
}

drogon::ContentType HttpFile::getContentType() const noexcept
{
    return implPtr_->getContentType();
}

const char *HttpFile::fileData() const noexcept
{
    return implPtr_->fileData();
}

std::string HttpFile::getMd5() const noexcept
{
    return implPtr_->getMd5();
}

const std::string &HttpFile::getContentTransferEncoding() const noexcept
{
    return implPtr_->getContentTransferEncoding();
}

HttpFile::HttpFile(std::shared_ptr<HttpFileImpl> &&implPtr) noexcept
    : implPtr_(std::move(implPtr))
{
}

const std::string &HttpFile::getItemName() const noexcept
{
    return implPtr_->getItemName();
}
