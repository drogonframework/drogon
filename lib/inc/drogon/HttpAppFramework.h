//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#pragma once

//#include <drogon/HttpRequest.h>
//#include <drogon/HttpResponse.h>
//#include <drogon/CacheMap.h>
//#include <drogon/Session.h>
#include <trantor/utils/NonCopyable.h>


#include <memory>
#include <string>
namespace drogon
{
    class HttpAppFramework:public trantor::NonCopyable
    {
    public:
        HttpAppFramework(const std::string &ip,uint16_t port);
        void run();
        ~HttpAppFramework();
    private:
        bool _run;
        std::unique_ptr<class HttpAppFrameworkImpl>  _implPtr;
    };
}
