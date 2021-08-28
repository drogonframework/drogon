![](https://github.com/an-tao/drogon/wiki/images/drogon-white.jpg)

[![Build Status](https://travis-ci.com/an-tao/drogon.svg?branch=master)](https://travis-ci.com/an-tao/drogon)
![Build Status](https://github.com/an-tao/drogon/workflows/Build%20Drogon/badge.svg?branch=master)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/alerts/)
[![Join the chat at https://gitter.im/drogon-web/community](https://badges.gitter.im/drogon-web/community.svg)](https://gitter.im/drogon-web/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Join the telegram group at https://t.me/joinchat/_mMNGv0748ZkMDAx](https://img.shields.io/badge/Telegram-2CA5E0?style=flat&logo=telegram&logoColor=white)](https://t.me/joinchat/_mMNGv0748ZkMDAx)
[![Docker image](https://img.shields.io/badge/Docker-image-blue.svg)](https://cloud.docker.com/u/drogonframework/repository/docker/drogonframework/drogon)

[English](./README.md) | 简体中文 | [繁體中文](./README.zh-TW.md)

**Drogon**是一个基于C++14/17的Http应用框架，使用Drogon可以方便的使用C++构建各种类型的Web应用服务端程序。
本版本库是github上[Drogon工程](https://github.com/an-tao/drogon)的镜像库。**Drogon**是作者非常喜欢的美剧《权力的游戏》中的一条龙的名字(汉译作卓耿)，和龙有关但并不是dragon的误写，为了不至于引起不必要的误会这里说明一下。

Drogon是一个跨平台框架，它支持Linux，也支持macOS、FreeBSD，OpenBSD，HaikuOS，和Windows。它的主要特点如下：

* 网络层使用基于epoll(macOS/FreeBSD下是kqueue)的非阻塞IO框架，提供高并发、高性能的网络IO。详细请见[TFB Tests Results](https://www.techempower.com/benchmarks/#section=data-r19&hw=ph&test=composite)；
* 全异步编程模式；
* 支持Http1.0/1.1(server端和client端)；
* 基于template实现了简单的反射机制，使主程序框架、控制器(controller)和视图(view)完全解耦；
* 支持cookies和内建的session；
* 支持后端渲染，把控制器生成的数据交给视图生成Html页面，视图由CSP模板文件描述，通过CSP标签把C++代码嵌入到Html页面，由drogon的命令行工具在编译阶段自动生成C++代码并编译；
* 支持运行期的视图页面动态加载(动态编译和加载so文件)；
* 非常方便灵活的路径(path)到控制器处理函数(handler)的映射方案；
* 支持过滤器(filter)链，方便在控制器之前执行统一的逻辑(如登录验证、Http Method约束验证等)；
* 支持https(基于OpenSSL实现);
* 支持websocket(server端和client端);
* 支持Json格式请求和应答, 对Restful API应用开发非常友好;
* 支持文件下载和上传,支持sendfile系统调用；
* 支持gzip/brotli压缩传输；
* 支持pipelining；
* 提供一个轻量的命令行工具drogon_ctl，帮助简化各种类的创建和视图代码的生成过程；
* 基于非阻塞IO实现的异步数据库读写，目前支持PostgreSQL和MySQL(MariaDB)数据库；
* 基于线程池实现sqlite3数据库的异步读写，提供与上文数据库相同的接口；
* 支持Redis异步读写；
* 支持ARM架构；
* 方便的轻量级ORM实现，支持常规的对象到数据库的双向映射操作；
* 支持插件，可通过配置文件在加载期动态拆装；
* 支持内建插入点的AOP
* 支持C++协程

## 一个非常简单的例子

不像大多数C++框架那样，drogon的主程序可以保持非常简单。 Drogon使用了一些小技巧使主程序和控制器解耦合. 控制器的路由设置可以在控制器类中定义或者配置文件中完成.

下面是一个典型的主程序的样子:

```c++
#include <drogon/drogon.h>
using namespace drogon;
int main()
{
    app().setLogPath("./")
         .setLogLevel(trantor::Logger::kWarn)
         .addListener("0.0.0.0", 80)
         .setThreadNum(16)
         .enableRunAsDaemon()
         .run();
}
```

如果使用配置文件，可以进一步简化成如下的样子:

```c++
#include <drogon/drogon.h>
using namespace drogon;
int main()
{
    app().loadConfigFile("./config.json").run();
}
```

当然，Drogon也提供了一些接口，使用户可以在main()函数中直接添加控制器逻辑，比如，用户可以注册一个lambda处理器到drogon框架中，如下所示：

```c++
app().registerHandler("/test?username={name}",
                    [](const HttpRequestPtr& req,
                       std::function<void (const HttpResponsePtr &)> &&callback,
                       const std::string &name)
                    {
                        Json::Value json;
                        json["result"]="ok";
                        json["message"]=std::string("hello,")+name;
                        auto resp=HttpResponse::newHttpJsonResponse(json);
                        callback(resp);
                    },
                    {Get,"LoginFilter"});
```


这看起来是很方便，但是这并不适用于复杂的应用，试想假如有数十个或者数百个处理函数要注册进框架，main()函数将膨胀到不可读的程度。显然，让每个包含处理函数的类在自己的定义中完成注册是更好的选择。所以，除非你的应用逻辑非常简单，我们不推荐使用上述接口，更好的实践是，我们可以创建一个HttpSimpleController对象，如下：


```c++
/// The TestCtrl.h file
#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class TestCtrl:public drogon::HttpSimpleController<TestCtrl>
{
public:
    virtual void asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    PATH_ADD("/test",Get);
    PATH_LIST_END
};

/// The TestCtrl.cc file
#include "TestCtrl.h"
void TestCtrl::asyncHandleHttpRequest(const HttpRequestPtr& req,
                                      std::function<void (const HttpResponsePtr &)> &&callback)
{
    //write your application logic here
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("<p>Hello, world!</p>");
    resp->setExpiredTime(0);
    callback(resp);
}
```

**上面程序的大部分代码都可以由`drogon_ctl`命令创建**（这个命令是`drogon_ctl create controller TestCtr`）。用户所需做的就是添加自己的业务逻辑。在这个例子中，当客户端访问URL`http://ip/test`时，控制器简单的返回了一个`Hello, world!`页面。

对于JSON格式的响应，我们可以像下面这样创建控制器：

```c++
/// The header file
#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class JsonCtrl : public drogon::HttpSimpleController<JsonCtrl>
{
  public:
    virtual void asyncHandleHttpRequest(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    //list path definitions here;
    PATH_ADD("/json", Get);
    PATH_LIST_END
};

/// The source file
#include "JsonCtrl.h"
void JsonCtrl::asyncHandleHttpRequest(const HttpRequestPtr &req,
                                      std::function<void(const HttpResponsePtr &)> &&callback)
{
    Json::Value ret;
    ret["message"] = "Hello, World!";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
}
```

让我们更进一步，通过HttpController类创建一个RESTful API的例子，如下所示（忽略了实现文件）：

```c++
/// The header file
#pragma once
#include <drogon/HttpController.h>
using namespace drogon;
namespace api
{
namespace v1
{
class User : public drogon::HttpController<User>
{
  public:
    METHOD_LIST_BEGIN
    //use METHOD_ADD to add your custom processing function here;
    METHOD_ADD(User::getInfo, "/{id}", Get);                  //path is /api/v1/User/{arg1}
    METHOD_ADD(User::getDetailInfo, "/{id}/detailinfo", Get);  //path is /api/v1/User/{arg1}/detailinfo
    METHOD_ADD(User::newUser, "/{name}", Post);                 //path is /api/v1/User/{arg1}
    METHOD_LIST_END
    //your declaration of processing function maybe like this:
    void getInfo(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, int userId) const;
    void getDetailInfo(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, int userId) const;
    void newUser(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&userName);
  public:
    User()
    {
        LOG_DEBUG << "User constructor!";
    }
};
} // namespace v1
} // namespace api
```

如你所见，通过`HttpController`类，用户可以同时映射路径和路径参数，这对RESTful API应用来说非常方便。

另外，你可以发现前面所有的处理函数接口都是异步的，处理器的响应是通过回调对象返回的。这种设计是出于对高性能的考虑，因为在异步模式下，可以使用少量的线程（比如和处理器核心数相等的线程）处理大量的并发请求。

编译上述的所有源文件后，我们得到了一个非常简单的web应用程序，这是一个不错的开始。**请访问[wiki](https://github.com/an-tao/drogon/wiki/CHN-01-概述)或者[doxiz](https://doxiz.com/drogon/master/overview/)以获取更多的信息**

## 贡献方式

欢迎您的贡献。 请阅读[贡献指南](CONTRIBUTING.md)以获取更多的信息。

## QQ交流群：1137909452

欢迎交流探讨。
