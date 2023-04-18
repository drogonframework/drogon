![](https://github.com/an-tao/drogon/wiki/images/drogon-white.jpg)

[![Build Status](https://github.com/an-tao/drogon/workflows/Build%20Drogon/badge.svg?branch=master)](https://github.com/drogonframework/drogon/actions)
[![Join the chat at https://gitter.im/drogon-web/community](https://badges.gitter.im/drogon-web/community.svg)](https://gitter.im/drogon-web/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Join the telegram group at https://t.me/joinchat/_mMNGv0748ZkMDAx](https://img.shields.io/badge/Telegram-2CA5E0?style=flat&logo=telegram&logoColor=white)](https://t.me/joinchat/_mMNGv0748ZkMDAx)
[![Docker image](https://img.shields.io/badge/Docker-image-blue.svg)](https://cloud.docker.com/u/drogonframework/repository/docker/drogonframework/drogon)

[English](./README.md) | [简体中文](./README.zh-CN.md) | 繁體中文

**Drogon**是一個基於C++14/17的Http應用框架，使用Drogon可以方便的使用C++構建各種類型的Web App伺服器程式。
本版本庫是github上[Drogon](https://github.com/an-tao/drogon)的鏡像庫。 **Drogon**是作者非常喜歡的美劇《冰與火之歌：權力遊戲》中的一條龍的名字(漢譯作卓耿)，和龍有關但並不是dragon的誤寫，為了不至於引起不必要的誤會這裡說明一下。

Drogon是一個跨平台框架，它支援Linux，也支援macOS、FreeBSD/OpenBSD、HaikuOS和Windows。它的主要特點如下：

* 網路層使用基於epoll(macOS/FreeBSD下是kqueue)的非阻塞IO框架，提供高並發、高性能的網路IO。詳細請見[TFB Tests Results](https://www.techempower.com/benchmarks/#section=data-r19&hw=ph&test=composite)；
* 全異步程式設計；
* 支援Http1.0/1.1(server端和client端)；
* 基於模板(template)實現了簡單的反射機制，使主程式框架、控制器(controller)和視圖(view)完全去耦；
* 支援cookies和內建的session；
* 支援後端渲染，把控制器生成的數據交給視圖生成Html頁面，視圖由CSP模板文件描述，通過CSP標籤把C++程式碼嵌入到Html頁面，由drogon的指令列工具在編譯階段自動生成C++程式碼並編譯；
* 支援運行期的視圖頁面動態加載(動態編譯和載入so文件)；
* 非常方便靈活的路徑(path)到控制器處理函數(handler)的映射方案；
* 支援過濾器(filter)鏈，方便在控制器之前執行統一的邏輯(如登錄驗證、Http Method約束驗證等)；
* 支援https(基於OpenSSL);
* 支援websocket(server端和client端);
* 支援Json格式的請求和回應, 方便開發Restful API;
* 支援文件下載和上傳,支援sendfile系統呼叫；
* 支援gzip/brotli壓縮傳輸；
* 支援pipelining；
* 提供一個輕量的指令列工具drogon_ctl，幫助簡化各種類的創造和視圖程式碼的生成過程；
* 非同步的讀寫資料庫，目前支援PostgreSQL和MySQL(MariaDB)資料庫；
* 支援異步讀寫Redis;
* 基於執行序池實現sqlite3資料庫的異步讀寫，提供與上文資料庫相同的接口；
* 支援ARM架構；
* 方便的輕量級ORM實現，一般物件到資料庫的雙向映射；
* 支援外掛，可通過設定文件在載入時動態載入；
* 支援內建插入點的AOP
* 支援C++ coroutine

## 一個非常簡單的例子

不像大多數C++框架那樣，drogon的主程式可以非常簡單。 Drogon使用了一些小技巧使主程式和控制器去耦. 控制器的路由設定可以在控制器類別中定義或者設定文件中完成.

下面是一個典型的主程式的樣子:

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

如果使用設定文件，可以進一步簡化成這樣:

```c++
#include <drogon/drogon.h>
using namespace drogon;
int main()
{
    app().loadConfigFile("./config.json").run();
}
```

當然，Drogon也提供了一些函數，使使用者可以在main()函數中直接添​​加控制器邏輯，比如，使用者可以註冊一個lambda處理器到drogon框架中，如下所示：

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


這看起來是很方便，但是這並不適用於復雜的場景，試想假如有數十個或者數百個處理函數要註冊進框架，main()函數將膨脹到不可讀的程度。顯然，讓每個包含處理函數的類在自己的定義中完成註冊是更好的選擇。所以，除非你的應用邏輯非常簡單，我們不推薦使用上述接口，更好的實踐是，我們可以創造一個HttpSimpleController類別，如下：


```c++
/// The TestCtrl.h file
#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class TestCtrl:public drogon::HttpSimpleController<TestCtrl>
{
public:
    void asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) override;
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

**上面程式的大部分程式碼都可以由`drogon_ctl`指令創造**（這個指令是`drogon_ctl create controller TestCtr`）。使用者所需做的就是添加自己的業務邏輯。在這個例子中，當客戶端訪問URL`http://ip/test`時，控制器簡單的回傳了一個`Hello, world!`頁面。

對於JSON格式的回應，我們可以像下面這樣創造控制器：

```c++
/// The header file
#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class JsonCtrl : public drogon::HttpSimpleController<JsonCtrl>
{
  public:
    void asyncHandleHttpRequest(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) override;
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

讓我們更進一步，通過HttpController類別創造一個RESTful API的例子，如下所示（忽略了實做文件）：

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

如你所見，通過`HttpController`類別，使用者可以同時映射路徑和路徑參數，這對RESTful API應用來說非常方便。

另外，你可以發現前面所有的處理函數接口都是異步的，處理器的回應是通過回調對象回傳的。這種設計是出於對高性能的考慮，因為在異步模式下，可以使用少量的執行序（比如和處理器核心數相等的執行序）處理大量的並發請求。

編譯上述的所有源文件後，我們得到了一個非常簡單的web應用程式，這是一個不錯的開始。 **請瀏覽[wiki](https://github.com/an-tao/drogon/wiki/CHN-01-概述)**

## 貢獻方式

歡迎您的貢獻。請閱讀[貢獻指南](CONTRIBUTING.md)以獲取更多的信息。

<a href="https://github.com/drogonframework/drogon/graphs/contributors"><img src="https://contributors-svg.opencollective.com/drogon/contributors.svg?width=890&button=false" alt="Code contributors" /></a>

## QQ交流群：1137909452

歡迎交流探討。
