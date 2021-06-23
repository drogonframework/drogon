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
            bool loggedIn =
                req->session()->getOptional<bool>("loggedIn").value_or(false);
            HttpResponsePtr resp;
            if (loggedIn == false)
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
            req->session()->erase("loggedIn");
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

            // NOTE: Do not use MD5 for the password hash under any
            // circumstances. We only use it because Drogon is not a
            // cryptography library, so it does not include a better hash
            // algorithm. Use Argon2 or BCrypt in a real product. username:
            // user, password: password123
            if (user == "user" && utils::getMd5("jadsjhdsajkh" + passwd) ==
                                      "5B5299CF4CEAE2D523315694B82573C9")
            {
                req->session()->insert("loggedIn", true);
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
        // All sessions are stored for 24 Hours
        .enableSession(24h)
        .addListener("127.0.0.1", 8848)
        .run();
}