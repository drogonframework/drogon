//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.


#pragma once

#include <drogon/DrClassMap.h>

#include <cxxabi.h>

namespace drogon
{
    class DrObjectBase
    {
    public:
        virtual const std::string & className()const {
            static std::string _name="DrObjectBase";
            return _name;
        }
    };

    /*
    * a class template to
    * implement the reflection function of creating the class object by class name
    * */
    template <typename T>
    class DrObject:public DrObjectBase
    {
    public:

        virtual const std::string & className()const override {
            return _alloc.className();
        }

    protected:
        //protect constructor to make the class only be inherited
        DrObject(){}
    private:
        class DrAllocator
        {
        public:
            DrAllocator():
                    _className(demangle(typeid(T).name()))
            {
                DrClassMap::registerClass(className(),[]()->DrObjectBase*{
                    T* p=new T;
                    return static_cast<DrObjectBase*>(p);
                });
            }
            const std::string & className() const {return _className;}

        private:
            std::string demangle( const char* mangled_name ) {

                std::size_t len = 0 ;
                int status = 0 ;
                std::unique_ptr< char, decltype(&std::free) > ptr(
                        __cxxabiv1::__cxa_demangle( mangled_name, nullptr, &len, &status ), &std::free ) ;
                return ptr.get() ;
            }
            std::string _className;
        };

        //use static val to register allocator function for class T;
        static DrAllocator _alloc;
    };
    template <typename T> typename DrObject<T>::DrAllocator DrObject<T>::_alloc;
}
