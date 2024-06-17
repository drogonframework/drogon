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
    auto path = req->path();
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
    auto end = trantor::Date::date();
    auto duration =
        end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch();
    if (promExporter)
    {
        auto collector =
            promExporter->getCollector<drogon::monitoring::Histogram>(
                "http_requests_duration");
        if (collector)
        {
            collector
                ->metric({method, path}, std::vector<double>{1, 2, 3}, 1h, 6)
                ->observe(duration);
        }
    }
    co_return resp;
}
