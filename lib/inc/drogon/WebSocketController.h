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

#include <drogon/DrObject.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/WebSocketConnection.h>
#include <trantor/utils/Logger.h>
#include <trantor/net/TcpConnection.h>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#define WS_PATH_LIST_BEGIN public:\
static std::vector<std::string> paths() \
{\
    std::vector<std::string> vet;

#define WS_PATH_ADD(path) \
    vet.push_back(path)

#define WS_PATH_LIST_END \
return vet;\
}
namespace drogon
{
    class WebSocketControllerBase:public virtual DrObjectBase
    {
    public:
        //on new data received
        virtual void handleNewMessage(const WebSocketConnectionPtr&,
                                      const std::string &message)=0;
        //on new connection or after disconnect
        virtual void handleConnection(const WebSocketConnectionPtr&)=0;
        virtual ~WebSocketControllerBase(){}
    };

    typedef std::shared_ptr<WebSocketControllerBase> WebSocketControllerBasePtr;
    template <typename T>
    class WebSocketController:public DrObject<T>,public WebSocketControllerBase
    {
    public:

        virtual ~WebSocketController(){}

    protected:

        WebSocketController(){}

    private:

        class pathRegister {
        public:
        pathRegister()
        {
            auto vPaths=T::paths();

            for(auto path:vPaths)
            {
                LOG_DEBUG<<"register websocket controller ("<<WebSocketController<T>::classTypeName()<<") on path:"<<path;
                HttpAppFramework::instance().registerWebSocketController(path,WebSocketController<T>::classTypeName());
            }

        }

        protected:
            void _register(const std::string& className,const std::vector<std::string> &paths)
            {
                for(auto reqPath:paths)
                {
                    std::cout<<"register controller class "<<className<<" on path "<<reqPath<<std::endl;
                }
            }
        };
        friend pathRegister;
        static pathRegister _register;
        virtual void* touch()
        {
            return &_register;
        }
    };
    template <typename T> typename WebSocketController<T>::pathRegister WebSocketController<T>::_register;

}
