/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once

#include <trantor/utils/NonCopyable.h>
#include <drogon/DrObject.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/PostFilter.h>
#include <drogon/GetFilter.h>
#include <drogon/PutFilter.h>
#include <drogon/DeleteFilter.h>
#include <drogon/version.h>
#include <drogon/NotFound.h>
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace drogon
{
    //the drogon banner
    const char banner[]=       "     _                             \n"
                               "  __| |_ __ ___   __ _  ___  _ __  \n"
                               " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
                               "| (_| | | | (_) | (_| | (_) | | | |\n"
                               " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
                               "                 |___/             \n";
    inline std::string getVersion()
    {
        return VERSION;
    }
    inline std::string getGitCommit()
    {
        return VERSION_MD5;
    }
    class HttpAppFramework:public trantor::NonCopyable
    {
    public:
        static HttpAppFramework &instance();
        virtual void addListener(const std::string &ip,uint16_t port)=0;
        virtual void run()=0;
        virtual ~HttpAppFramework();
        virtual void registerHttpSimpleController(const std::string &pathName,const std::string &crtlName,const std::vector<std::string> &filters=
                std::vector<std::string>())=0;
        virtual void enableSession(const size_t timeout=0)=0;
        virtual void disableSession()=0;
    private:
        static std::once_flag _once;
        static class HttpAppFrameworkImpl *_implPtr;
    };
}
