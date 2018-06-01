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
#include <trantor/utils/Logger.h>
#include <string>
#include <vector>
#include <iostream>

namespace drogon
{
    template <typename T>
    class HttpApiController:public DrObject<T>
    {

    };
}