# Redis example

An example for using coroutines of redis clients. This example is a redis implementation of
the [local sessions example](https://github.com/drogonframework/drogon/wiki/ENG-07-Session#examples-of-sessions) on wiki.

## Usage

First of all you need redis running on the port 6379;

Please use new compilers that support coroutines. Configure the project with the following
command:

```shell
cmake .. -DCMAKE_CXX_FLAGS="-std=c++20 -fcoroutines" #for GCC 10
cmake .. -DCMAKE_CXX_FLAGS="-std=c++20" #for GCC >= 11
cmake .. -DCMAKE_CXX_FLAGS="/std:c++20" #for MSVC >= 16.25
```
