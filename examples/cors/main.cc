#include <drogon/HttpAppFramework.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>
#include "trantor/utils/Logger.h"

using namespace drogon;

/// Configure Cross-Origin Resource Sharing (CORS) support.
///
///  This function registers both synchronous pre-processing advice for handling
///  OPTIONS preflight requests and post-handling advice to inject CORS headers
///  into all responses dynamically based on the incoming request headers.
void setupCors()
{
    // Register sync advice to handle CORS preflight (OPTIONS) requests
    drogon::app().registerSyncAdvice([](const drogon::HttpRequestPtr &req)
                                         -> drogon::HttpResponsePtr {
        if (req->method() == drogon::HttpMethod::Options)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();

            // Set Access-Control-Allow-Origin header based on the Origin
            // request header
            const auto &origin = req->getHeader("Origin");
            if (!origin.empty())
            {
                resp->addHeader("Access-Control-Allow-Origin", origin);
            }

            // Set Access-Control-Allow-Methods based on the requested method
            const auto &requestMethod =
                req->getHeader("Access-Control-Request-Method");
            if (!requestMethod.empty())
            {
                resp->addHeader("Access-Control-Allow-Methods", requestMethod);
            }

            // Allow credentials to be included in cross-origin requests
            resp->addHeader("Access-Control-Allow-Credentials", "true");

            // Set allowed headers from the Access-Control-Request-Headers
            // header
            const auto &requestHeaders =
                req->getHeader("Access-Control-Request-Headers");
            if (!requestHeaders.empty())
            {
                resp->addHeader("Access-Control-Allow-Headers", requestHeaders);
            }

            return std::move(resp);
        }
        return {};
    });

    // Register post-handling advice to add CORS headers to all responses
    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr &req,
           const drogon::HttpResponsePtr &resp) -> void {
            // Set Access-Control-Allow-Origin based on the Origin request
            // header
            const auto &origin = req->getHeader("Origin");
            if (!origin.empty())
            {
                resp->addHeader("Access-Control-Allow-Origin", origin);
            }

            // Reflect the requested Access-Control-Request-Method back in the
            // response
            const auto &requestMethod =
                req->getHeader("Access-Control-Request-Method");
            if (!requestMethod.empty())
            {
                resp->addHeader("Access-Control-Allow-Methods", requestMethod);
            }

            // Allow credentials to be included in cross-origin requests
            resp->addHeader("Access-Control-Allow-Credentials", "true");

            // Reflect the requested Access-Control-Request-Headers back
            const auto &requestHeaders =
                req->getHeader("Access-Control-Request-Headers");
            if (!requestHeaders.empty())
            {
                resp->addHeader("Access-Control-Allow-Headers", requestHeaders);
            }
        });
}

/**
 * Main function to start the Drogon application with CORS-enabled routes.
 * This example includes:
 * - A simple GET endpoint `/hello` that returns a greeting message.
 * - A POST endpoint `/echo` that echoes back the request body.
 *  You can test with curl to test the CORS support:
 *
```
    curl -i -X OPTIONS http://localhost:8000/echo \
        -H "Origin: http://localhost:3000" \
        -H "Access-Control-Request-Method: POST" \
        -H "Access-Control-Request-Headers: Content-Type"
```
or
```
    curl -i -X POST http://localhost:8000/echo \
        -H "Origin: http://localhost:3000" \
        -H "Content-Type: application/json" \
        -d '{"key":"value"}'
```
 */
int main()
{
    // Listen on port 8000 for all interfaces
    app().addListener("0.0.0.0", 8000);

    // Setup CORS support
    setupCors();

    // Register /hello route for GET and OPTIONS methods
    app().registerHandler(
        "/hello",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody("Hello from Drogon!");

            // Log client IP address
            LOG_INFO << "Request to /hello from " << req->getPeerAddr().toIp();

            callback(resp);
        },
        {Get, Options});

    // Register /echo route for POST and OPTIONS methods
    app().registerHandler(
        "/echo",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(std::string("Echo: ").append(req->getBody()));

            // Log client IP and request body
            LOG_INFO << "Request to /echo from " << req->getPeerAddr().toIp();
            LOG_INFO << "Echo content: " << req->getBody();

            callback(resp);
        },
        {Post, Options});

    // Start the application main loop
    app().run();

    return 0;
}
