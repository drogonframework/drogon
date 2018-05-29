#pragma once

#include <drogon/DrObject.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpViewBase.h>
namespace drogon {
    template <typename T>
    class HttpView:public DrObject<T>,public HttpViewBase
    {
    protected:
        HttpView(){}
    };
}
