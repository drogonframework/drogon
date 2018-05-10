//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#pragma once

#include <drogon/DrObject.h>

namespace drogon
{
    template <typename T typename ...>
    class HttpSimpleController:public DrObject<T>
    {

    };
}
