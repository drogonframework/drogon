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

#include <drogon/utils/Utilities.h>
#include <drogon/HttpApiBinder.h>
#include <trantor/utils/NonCopyable.h>
#include <drogon/DrObject.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/PostFilter.h>
#include <drogon/GetFilter.h>
#include <drogon/PutFilter.h>
#include <drogon/DeleteFilter.h>
#include <drogon/LocalHostFilter.h>
#include <drogon/version.h>
#include <drogon/NotFound.h>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoop.h>
#include <drogon/CacheMap.h>
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
        virtual void setThreadNum(size_t threadNum)=0;
        virtual void setSSLFiles(const std::string &certPath,
                                 const std::string &keyPath)=0;
        virtual void addListener(const std::string &ip,uint16_t port,bool useSSL=false)=0;
        virtual void run()=0;
        virtual ~HttpAppFramework();
        virtual void registerWebSocketController(const std::string &pathName,
                                                  const std::string &crtlName,
                                                 const std::vector<std::string> &filters=
                                                 std::vector<std::string>())=0;
        virtual void registerHttpSimpleController(const std::string &pathName,
                                                  const std::string &crtlName,
                                                  const std::vector<std::string> &filters=std::vector<std::string>())=0;
        virtual void registerHttpApiController(const std::string &pathPattern,
                                               const HttpApiBinderBasePtr &binder,
                                               const std::vector<std::string> &filters=std::vector<std::string>())=0;
        template <typename FUNCTION>
        static void registerHttpApiMethod(const std::string &pathPattern,
                                          FUNCTION && function,
                                          const std::vector<std::string> &filters=std::vector<std::string>())
        {
            LOG_TRACE<<"pathPattern:"<<pathPattern;
            HttpApiBinderBasePtr binder;

            binder=std::make_shared<
                    HttpApiBinder<FUNCTION>
            >(std::forward<FUNCTION>(function));

            instance().registerHttpApiController(pathPattern,binder,filters);
        }
        virtual void enableSession(const size_t timeout=0)=0;
        virtual void disableSession()=0;
        virtual const std::string & getDocumentRoot() const =0;
        virtual void setDocumentRoot(const std::string &rootPath)=0;
        virtual void setFileTypes(const std::vector<std::string> &types)=0;
        virtual void enableDynamicSharedLibLoading(const std::vector<std::string> &libPaths)=0;
        virtual trantor::EventLoop *loop()=0;
    };
}
