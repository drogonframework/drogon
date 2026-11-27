![](https://github.com/an-tao/drogon/wiki/images/drogon-white17.jpg)

[![Build Status](https://github.com/drogonframework/drogon/actions/workflows/cmake.yml/badge.svg?branch=master)](https://github.com/drogonframework/drogon/actions)
[![Conan Center](https://img.shields.io/conan/v/drogon)](https://conan.io/center/recipes/drogon)
[![Join the telegram group at https://t.me/joinchat/_mMNGv0748ZkMDAx](https://img.shields.io/badge/Telegram-2CA5E0?style=flat&logo=telegram&logoColor=white)](https://t.me/joinchat/_mMNGv0748ZkMDAx)
[![Discord](https://img.shields.io/badge/Discord-Join%20our%20Discord-5865F2?logo=discord&logoColor=white)](https://discord.com/invite/3DvHY6Ewuj)
[![Docker image](https://img.shields.io/badge/Docker-image-blue.svg)](https://cloud.docker.com/u/drogonframework/repository/docker/drogonframework/drogon)

[English](./README.md) | [简体中文](./README.zh-CN.md) | [繁體中文](./README.zh-TW.md) | 한국어

### 개요

**Drogon**은 C++17/20 기반의 HTTP 애플리케이션 프레임워크입니다. Drogon을 사용하면 C++로 다양한 유형의 웹 애플리케이션 서버 프로그램을 손쉽게 만들 수 있습니다. **Drogon**은 제가 매우 좋아하는 미국 TV 시리즈 *왕좌의 게임*에 등장하는 용의 이름입니다.

Drogon은 크로스 플랫폼 프레임워크로 Linux, macOS, FreeBSD, OpenBSD, HaikuOS, Windows를 지원합니다. 주요 기능은 다음과 같습니다.

* epoll(macOS/FreeBSD에서는 kqueue) 기반의 논블로킹 I/O 네트워크 라이브러리를 사용하여 높은 동시성과 고성능 네트워크 I/O를 제공합니다. 자세한 내용은 [TFB 테스트 결과](https://www.techempower.com/benchmarks/#section=data-r19&hw=ph&test=composite)를 참고하세요.
* 완전한 비동기 프로그래밍 모델을 제공합니다.
* HTTP 1.0/1.1(서버 및 클라이언트)을 지원합니다.
* 템플릿 기반의 간단한 리플렉션 메커니즘으로 메인 프로그램 프레임워크, 컨트롤러, 뷰를 완전히 분리합니다.
* 쿠키와 내장 세션을 지원합니다.
* 백엔드 렌더링을 지원합니다. 컨트롤러가 뷰에 데이터를 전달해 HTML 페이지를 만들며, 뷰는 CSP 템플릿 파일로 기술합니다. CSP 태그를 통해 HTML 페이지에 C++ 코드를 삽입하고, drogon 명령줄 도구가 컴파일용 C++ 코드 파일을 자동으로 생성합니다.
* 뷰 페이지의 동적 로딩(실행 시간 컴파일 및 로딩)을 지원합니다.
* 경로에서 컨트롤러 핸들러로 연결하는 편리하고 유연한 라우팅을 제공합니다.
* HTTP 요청을 처리하기 전에 로그인 확인, HTTP 메서드 제약 확인 등의 공통 로직을 실행하기 위한 필터 체인을 지원합니다.
* HTTPS(OpenSSL 기반)를 지원합니다.
* WebSocket(서버 및 클라이언트)을 지원합니다.
* JSON 형식의 요청과 응답을 지원하여 RESTful API 애플리케이션 개발에 친화적입니다.
* 파일 다운로드와 업로드를 지원합니다.
* gzip 및 brotli 압축 전송을 지원합니다.
* 파이프라이닝을 지원합니다.
* Drogon의 다양한 클래스 생성 및 뷰 코드 생성을 간소화하는 경량 명령줄 도구 `drogon_ctl`을 제공합니다.
* 논블로킹 I/O 기반의 비동기 데이터베이스 읽기 및 쓰기를 지원하며 PostgreSQL과 MySQL(MariaDB)을 지원합니다.
* 스레드 풀 기반의 sqlite3 데이터베이스 비동기 읽기 및 쓰기를 지원합니다.
* Redis 비동기 읽기 및 쓰기를 지원합니다.
* ARM 아키텍처를 지원합니다.
* 일반적인 객체-데이터베이스 양방향 매핑을 지원하는 편리하고 가벼운 ORM 구현을 제공합니다.
* 로드 시 설정 파일로 설치할 수 있는 플러그인을 지원합니다.
* 내장 조인 포인트를 이용한 AOP를 지원합니다.
* C++ 코루틴을 지원합니다.

## 매우 간단한 예제

대부분의 C++ 프레임워크와 달리 drogon 애플리케이션의 메인 프로그램은 깔끔하고 단순하게 유지할 수 있습니다. Drogon은 몇 가지 기법으로 컨트롤러를 메인 프로그램에서 분리합니다. 컨트롤러의 라우팅 설정은 매크로나 설정 파일을 통해 할 수 있습니다.

다음은 일반적인 drogon 애플리케이션의 메인 프로그램입니다.

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

설정 파일을 사용하면 다음과 같이 더 간단하게 만들 수 있습니다.

```c++
#include <drogon/drogon.h>

using namespace drogon;

int main()
{
    app().loadConfigFile("./config.json").run();
}
```

Drogon은 `main()` 함수에서 컨트롤러 로직을 직접 추가할 수 있는 인터페이스도 제공합니다. 예를 들어 다음과 같이 핸들러를 등록할 수 있습니다.

```c++
app().registerHandler("/test?username={name}",
                    [](const HttpRequestPtr& req,
                       std::function<void (const HttpResponsePtr &)> &&callback,
                       const std::string &name) -> void
                    {
                        Json::Value json;
                        json["result"] = "ok";
                        json["message"] = "hello, " + name;
                        HttpResponsePtr resp = HttpResponse::newHttpJsonResponse(json);
                        callback(resp);
                    },
                    {Get,"LoginFilter"});
```

이러한 인터페이스는 직관적으로 보이지만 복잡한 비즈니스 로직에는 적합하지 않습니다. 프레임워크에 등록할 핸들러가 수십 또는 수백 개라면 각각의 클래스에 따로 구현하는 편이 낫습니다. 따라서 로직이 매우 단순한 경우가 아니라면 위 인터페이스 대신 다음과 같이 `HttpSimpleController`를 만드는 것을 권장합니다.

```c++
/// The TestCtrl.h file
#pragma once
#include <drogon/HttpSimpleController.h>

using namespace drogon;

class TestCtrl : public HttpSimpleController<TestCtrl>
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
    // write your application logic here
    HttpResponsePtr resp = HttpResponse::newHttpResponse();
    resp->setBody("<p>Hello, world!</p>");
    resp->setExpiredTime(0);
    callback(resp);
}
```

**위 프로그램의 대부분은 drogon이 제공하는 명령줄 도구 `drogon_ctl`로 자동 생성할 수 있습니다**(`drogon_ctl create controller TestCtrl`). 사용자는 자신의 비즈니스 로직만 추가하면 됩니다. 이 예제에서는 클라이언트가 `http://ip/test` URL에 접근하면 컨트롤러가 `Hello, world!` 문자열을 반환합니다.

JSON 형식 응답을 위한 컨트롤러는 다음과 같이 만들 수 있습니다.

```c++
/// The header file
#pragma once

#include <drogon/HttpSimpleController.h>

using namespace drogon;

class JsonCtrl : public HttpSimpleController<JsonCtrl>
{
  public:
    void asyncHandleHttpRequest(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definitions here;
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
    HttpResponsePtr resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
}
```

한 단계 더 나아가 `HttpController` 클래스로 RESTful API 예제를 만들 수 있습니다(소스 파일은 생략).

```c++
/// The header file
#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

namespace api::v1
{
class User : public HttpController<User>
{
  public:
    METHOD_LIST_BEGIN
    // use METHOD_ADD to add your custom processing function here;
    METHOD_ADD(User::getInfo, "/{id}", Get); // path is /api/v1/User/{arg1}
    METHOD_ADD(User::getDetailInfo, "/{id}/detailinfo", Get); // path is /api/v1/User/{arg1}/detailinfo
    METHOD_ADD(User::newUser, "/{name}", Post); // path is /api/v1/User/{arg1}
    METHOD_LIST_END
    // your declaration of processing function maybe like this:
    void getInfo(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, int userId) const;
    void getDetailInfo(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, int userId) const;
    void newUser(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&userName);
  public:
    User()
    {
        LOG_DEBUG << "User constructor!";
    }
};
} // namespace api::v1
```

보시다시피 `HttpController`를 사용하면 경로와 경로 매개변수를 동시에 매핑할 수 있어 RESTful API 애플리케이션에 매우 편리합니다.

또한 앞선 모든 핸들러 인터페이스가 비동기 모드이며 콜백 객체를 통해 응답을 반환한다는 점을 알 수 있습니다. 이 설계는 성능을 위한 것으로, 비동기 모드에서는 적은 수의 스레드로 많은 동시 요청을 처리할 수 있습니다.

위의 모든 소스 파일을 컴파일하면 매우 간단한 웹 애플리케이션이 만들어집니다. 좋은 출발점이 될 것입니다. **더 자세한 내용은 GitHub의 [문서](https://drogonframework.github.io/drogon-docs/#/)를 참고하세요.**

## 크로스 컴파일

Drogon은 크로스 컴파일을 지원합니다. 예를 들어 툴체인 파일에서 `CMAKE_SYSTEM_NAME`을 정의해야 합니다.

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
```

툴체인 파일에서 `BUILD_EXAMPLES` 및 `BUILD_CTL`을 `OFF`로 설정하면 예제와 drogon_ctl 빌드를 비활성화할 수 있습니다.

## 빌드 옵션

Drogon은 여러 빌드 옵션을 제공합니다. cmake 명령줄, cmake 파일 등에서 해당 변수를 `ON` 또는 `OFF`로 설정해 활성화하거나 비활성화할 수 있습니다.

| 옵션 이름 | 설명 | 기본값 |
| :--- | :--- | :--- |
| BUILD_CTL | drogon_ctl 빌드 | ON |
| BUILD_EXAMPLES | 예제 빌드 | ON |
| BUILD_ORM | ORM 빌드 | ON |
| COZ_PROFILING | 프로파일링에 coz 사용 | OFF |
| BUILD_SHARED_LIBS | drogon을 공유 라이브러리로 빌드 | OFF |
| BUILD_DOC | Doxygen 문서 빌드 | OFF |
| BUILD_BROTLI | Brotli 빌드 | ON |
| BUILD_YAML_CONFIG | YAML 설정 빌드 | ON |
| USE_SUBMODULE | trantor를 서브모듈로 사용 | ON |

## 기여

이 프로젝트는 코드로 기여해 주시는 모든 분 덕분에 존재합니다.

<a href="https://github.com/drogonframework/drogon/graphs/contributors"><img src="https://contributors-svg.opencollective.com/drogon/contributors.svg?width=890&button=false" alt="Code contributors" /></a>

모든 기여를 환영합니다. 자세한 내용은 [기여 가이드](CONTRIBUTING.md)를 참고하세요.
