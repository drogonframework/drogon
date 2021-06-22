#include <drogon/drogon.h>
#include <chrono>
using namespace drogon;
using namespace std::chrono_literals;

int main()
{
    app().registerHandler(
        "/",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            bool logined =
                req->session()->getOptional<bool>("logined").value_or(false);
            HttpResponsePtr resp;
            if (logined == false)
                resp = HttpResponse::newHttpViewResponse("LoginPage");
            else
                resp = HttpResponse::newHttpViewResponse("LogoutPage");
            callback(resp);
        });

    app().registerHandler(
        "/logout",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            HttpResponsePtr resp = HttpResponse::newHttpResponse();
            req->session()->erase("logined");
            resp->setBody("<script>window.location.href = \"/\";</script>");
            callback(resp);
        },
        {Post});

    app().registerHandler(
        "/login",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            HttpResponsePtr resp = HttpResponse::newHttpResponse();
            std::string user = req->getParameter("user");
            std::string passwd = req->getParameter("passwd");

            // NOTE: Do not, ever, use MD5 for password hash. We only use it
            // because Drogon is not a cryptography library, so doesn't come
            // with a better hash. Use Argon2 or BCrypt in a real product.
            // username: user, pasword: password123
            if (user == "user" && utils::getMd5("jadsjhdsajkh" + passwd) ==
                                      "5B5299CF4CEAE2D523315694B82573C9")
            {
                req->session()->insert("logined", true);
                resp->setBody("<script>window.location.href = \"/\";</script>");
                callback(resp);
            }
            else
            {
                resp->setStatusCode(k401Unauthorized);
                resp->setBody("<script>window.location.href = \"/\";</script>");
                callback(resp);
            }
        },
        {Post});

    LOG_INFO << "Server running on 127.0.0.1:8848";
    app()
        // All sessions are good for 24 Hrs
        .enableSession(24h)
        .addListener("127.0.0.1", 8848)
        .run();
}