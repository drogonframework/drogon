/**
 *
 *  ClassTraits.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once
#include <type_traits>

namespace drogon
{
namespace internal
{
/// This template is used to check whether S is a subclass of B.
template <typename S, typename B>
struct IsSubClass
{
    typedef typename std::remove_cv<typename std::remove_reference<S>::type>::type SubType;
    typedef typename std::remove_cv<typename std::remove_reference<B>::type>::type BaseType;
    static char test(void *)
    {
        return 0;
    }
    static int test(BaseType *)
    {
        return 0;
    }
    static const bool value = (sizeof(test((SubType *)nullptr)) == sizeof(int));
};

}  // namespace internal
}  // namespace drogon