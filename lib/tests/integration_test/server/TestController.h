#pragma once
#include <drogon/HttpSimpleController.h>
#include <drogon/IOThreadStorage.h>
#include <drogon/utils/monitoring/Counter.h>
#include <drogon/utils/monitoring/Collector.h>
#include <drogon/plugins/PromExporter.h>
using namespace drogon;

namespace example
{
class TestController : public drogon::HttpSimpleController<TestController>
{
  public:
    void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definitions here;
    // PATH_ADD("/path","filter1","filter2",...);
    PATH_ADD("/", Get);
    PATH_ADD("/Test", "nonFilter");
    PATH_ADD("/tpost", Post, Options);
    PATH_ADD("/slow", "TimeFilter", Get);

    PATH_LIST_END
    TestController()
    {
        LOG_DEBUG << "TestController constructor";
        auto collector = std::make_shared<
            drogon::monitoring::Collector<drogon::monitoring::Counter>>(
            "test_counter",
            "The counter for requests to the root url",
            std::vector<std::string>());
        counter_ = collector->metric(std::vector<std::string>());
        collector->registerTo(
            *app().getSharedPlugin<drogon::plugin::PromExporter>());
    }

  private:
    drogon::IOThreadStorage<int> threadIndex_;
    std::shared_ptr<drogon::monitoring::Counter> counter_;
};
}  // namespace example
