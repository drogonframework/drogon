#pragma once

#include <drogon/DrObject.h>
namespace drogon {
    class HttpViewBase;
    template <typename T>
    class HttpView:public DrObject<T>,public HttpViewBase
    {
    protected:
        HttpView(){}
    };
}
