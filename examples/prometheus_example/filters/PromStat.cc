/**
 *
 *  PromStat.cc
 *
 */

#include "PromStat.h"
#include <drogon/plugins/PromExporter.h>
using namespace drogon;

void Task<HttpResponsePtr> PromStat::invoke(const HttpRequestPtr &req,
                                            MiddlewareNextAwaiter &&next)
{
    auto path = req->path();
    auto method = req->methodString();
    auto promExporter = app().getPlugin<plugin::PromExporter>();
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
            collector->metric({method, path})->observe(duration);
        }
    }
    co_return resp;
}
