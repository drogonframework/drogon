#include <drogon/HttpController.h>
#include <drogon/HttpSimpleController.h>
#include <drogon/WebSocketController.h>
#include <drogon/drogon_test.h>

class Ctrl : public drogon::HttpController<Ctrl, false>
{
  public:
    static void initPathRouting()
    {
        created = true;
    };

    static bool created;
};

class SimpleCtrl : public drogon::HttpController<Ctrl, false>
{
  public:
    static void initPathRouting()
    {
        created = true;
    };

    static bool created;
};

class WsCtrl : public drogon::WebSocketController<WsCtrl, false>
{
  public:
    static void initPathRouting()
    {
        created = true;
    };

    static bool created;
};

bool Ctrl::created = false;
bool SimpleCtrl::created = false;
bool WsCtrl::created = false;

DROGON_TEST(ControllerCreation)
{
    REQUIRE(Ctrl::created == false);
    REQUIRE(SimpleCtrl::created == false);
    REQUIRE(WsCtrl::created == false);
}