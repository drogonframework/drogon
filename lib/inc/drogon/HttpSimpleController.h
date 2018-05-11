//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#pragma once

#include <drogon/DrObject.h>
#include <string>
#include <vector>
#include <iostream>
namespace drogon
{
    template <typename T>
    class HttpSimpleController:public DrObject<T>
    {
    public:
        HttpSimpleController(){}
        virtual ~HttpSimpleController(){}
    private:

        class pathRegister {
        public:
        pathRegister(){
//            std::cout<<"pathRegister!!"<<std::endl;
//            _register("hahaha",{std::string((char *)PATH)...});

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
        static pathRegister _register;
        virtual void* touch()
        {
            return &_register;
        }
    };
    template <typename T> typename HttpSimpleController<T>::pathRegister HttpSimpleController<T>::_register;

}
