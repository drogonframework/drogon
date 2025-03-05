/**
 *
 *  PromStat.cc
 *
 */

#include "PromStat.h"
#include <drogon/plugins/PromExporter.h>
#include <drogon/utils/monitoring/Counter.h>
#include <drogon/utils/monitoring/Histogram.h>
#include <drogon/HttpAppFramework.h>
#include <chrono>

using namespace std::literals::chrono_literals;
using namespace drogon;

Task<HttpResponsePtr> PromStat::invoke(const HttpRequestPtr &req,
                                       MiddlewareNextAwaiter &&next)
{
    std::string path{req->matchedPathPattern()};
    auto method = req->methodString();
    auto promExporter = app().getPlugin<drogon::plugin::PromExporter>();
    if (promExporter)
    {
        auto collector =
            promExporter->getCollector<drogon::monitoring::Counter>(
                "http_requests_total");
        if (collector)
        {
            collector->metric({method, path})->increment();
        }
    }
    auto start = trantor::Date::date();
    auto resp = co_await next;
    if (promExporter)
    {
        auto collector =
            promExporter->getCollector<drogon::monitoring::Histogram>(
                "http_request_duration_seconds");
        if (collector)
        {
            static const std::vector<double> boundaries{
                0.0001, 0.001, 0.01, 0.1, 0.5, 1, 2, 3};
            auto end = trantor::Date::date();
            auto duration =
                end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch();
            collector->metric({method, path}, boundaries, 1h, 6)
                ->observe((double)duration / 1000000);
        }
    }
    co_return resp;
}
