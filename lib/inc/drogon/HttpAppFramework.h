//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <trantor/utils/NonCopyable.h>
#include <string>
#include <set>
namespace drogon
{
    class HttpAppFramework:public trantor::NonCopyable
    {
    public:
        HttpAppFramework()= delete;
        HttpAppFramework(const std::string &ip,uint16_t port);
        void run();

    private:

        std::string _ip;
        uint16_t _port;
        void onAsyncRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback);
        void readSendFile(const std::string& filePath,const HttpRequest& req, HttpResponse* resp);
#ifdef USE_UUID
        //if uuid package found,we can use a uuid string as session id;
        uint _sessionTimeout=0;
#endif
        bool _enableLastModify=true;
        std::set<std::string> _fileTypeSet={"html","jpg"};
        std::string _rootPath;



        //tool funcs
#ifdef USE_UUID
        std::string getUuid();
        std::string stringToHex(unsigned char* ptr, long long length);
#endif
    };
}
