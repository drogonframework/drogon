![](https://github.com/an-tao/drogon/wiki/images/drogon-white.jpg)

[![Build Status](https://travis-ci.com/an-tao/drogon.svg?branch=master)](https://travis-ci.com/an-tao/drogon)
[![Build status](https://ci.appveyor.com/api/projects/status/12ffuf6j5vankgyb/branch/master?svg=true)](https://ci.appveyor.com/project/an-tao/drogon/branch/master)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/alerts/)
[![Join the chat at https://gitter.im/drogon-web/community](https://badges.gitter.im/drogon-web/community.svg)](https://gitter.im/drogon-web/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Docker image](https://img.shields.io/badge/Docker-image-blue.svg)](https://cloud.docker.com/u/drogonframework/repository/docker/drogonframework/drogon)

[English](./README.md) | [简体中文](./README.zh-CN.md) | 繁體中文

**Drogon**是一個基於C++14/17的Http應用框架，使用Drogon可以方便的使用C++構建各種類型的Web應用服務端程序。
本版本庫是github上[Drogon工程](https://github.com/an-tao/drogon)的鏡像庫。 **Drogon**是作者非常喜歡的美劇《權力的遊戲》中的一條龍的名字(漢譯作卓耿)，和龍有關但並不是dragon的誤寫，為了不至於引起不必要的誤會這裡說明一下。

Drogon是一個跨平台框架，它支持Linux，也支持macOS、FreeBSD，和Windows。它的主要特點如下：

* 網絡層使用基於epoll(macOS/FreeBSD下是kqueue)的非阻塞IO框架，提供高並發、高性能的網絡IO。詳細請見[TFB Tests Results](https://www.techempower.com/benchmarks/#section=data-r19&hw=ph&test=composite)；
* 全異步編程模式；
* 支持Http1.0/1.1(server端和client端)；
* 基於template實現了簡單的反射機制，使主程序框架、控制器(controller)和視圖(view)完全解耦；
* 支持cookies和內建的session；
* 支持後端渲染，把控制器生成的數據交給視圖生成Html頁面，視圖由CSP模板文件描述，通過CSP標籤把C++代碼嵌入到Html頁面，由drogon的命令行工具在編譯階段自動生成C++代碼並編譯；
* 支持運行期的視圖頁面動態加載(動態編譯和加載so文件)；
* 非常方便靈活的路徑(path)到控制器處理函數(handler)的映射方案；
* 支持過濾器(filter)鏈，方便在控制器之前執行統一的邏輯(如登錄驗證、Http Method約束驗證等)；
* 支持https(基於OpenSSL實現);
* 支持websocket(server端和client端);
* 支持Json格式請求和應答, 對Restful API應用開發非常友好;
* 支持文件下載和上傳,支持sendfile系統調用；
* 支持gzip/brotli壓縮傳輸；
* 支持pipelining；
* 提供一個輕量的命令行工具drogon_ctl，幫助簡化各種類的創建和視圖代碼的生成過程；
* 基於非阻塞IO實現的異步數據庫讀寫，目前支持PostgreSQL和MySQL(MariaDB)數據庫；
* 基於線程池實現sqlite3數據庫的異步讀寫，提供與上文數據庫相同的接口；
* 支持ARM架構；
* 方便的輕量級ORM實現，支持常規的對像到數據庫的雙向映射操作；
* 支持插件，可通過配置文件在加載期動態拆裝；
* 支持內建插入點的AOP

## 一個非常簡單的例子

不像大多數C++框架那樣，drogon的主程序可以保持非常簡單。 Drogon使用了一些小技巧使主程序和控制器解耦合. 控制器的路由設置可以在控制器類中定義或者配置文件中完成.

下面是一個典型的主程序的樣子:

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

如果使用配置文件，可以進一步簡化成如下的樣子:

```c++
#include <drogon/drogon.h>
using namespace drogon;
int main()
{
    app().loadConfigFile("./config.json").run();
}
```

當然，Drogon也提供了一些接口，使用戶可以在main()函數中直接添​​加控制器邏輯，比如，用戶可以註冊一個lambda處理器到drogon框架中，如下所示：

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


這看起來是很方便，但是這並不適用於復雜的應用，試想假如有數十個或者數百個處理函數要註冊進框架，main()函數將膨脹到不可讀的程度。顯然，讓每個包含處理函數的類在自己的定義中完成註冊是更好的選擇。所以，除非你的應用邏輯非常簡單，我們不推薦使用上述接口，更好的實踐是，我們可以創建一個HttpSimpleController對象，如下：


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

**上面程序的大部分代碼都可以由`drogon_ctl`命令創建**（這個命令是`drogon_ctl create controller TestCtr`）。用戶所需做的就是添加自己的業務邏輯。在這個例子中，當客戶端訪問URL`http://ip/test`時，控制器簡單的返回了一個`Hello, world!`頁面。

對於JSON格式的響應，我們可以像下面這樣創建控制器：

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

讓我們更進一步，通過HttpController類創建一個RESTful API的例子，如下所示（忽略了實現文件）：

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

如你所見，通過`HttpController`類，用戶可以同時映射路徑和路徑參數，這對RESTful API應用來說非常方便。

另外，你可以發現前面所有的處理函數接口都是異步的，處理器的響應是通過回調對象返回的。這種設計是出於對高性能的考慮，因為在異步模式下，可以使用少量的線程（比如和處理器核心數相等的線程）處理大量的並發請求。

編譯上述的所有源文件後，我們得到了一個非常簡單的web應用程序，這是一個不錯的開始。 **請訪問[wiki](https://github.com/an-tao/drogon/wiki/CHN-01-概述)或者[doxiz](https://doxiz.com/drogon/master/overview/)以獲取更多的信息**

## 貢獻方式

歡迎您的貢獻。請閱讀[貢獻指南](CONTRIBUTING.md)以獲取更多的信息。

## QQ交流群：1137909452

歡迎交流探討。