#include <drogon/plugins/PromExporter.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/utils/monitoring/Collector.h>

using namespace drogon;
using namespace drogon::monitoring;
using namespace drogon::plugin;

void PromExporter::initAndStart(const Json::Value &config)
{
    path_ = config.get("path", path_).asString();
    auto &app = drogon::app();
    std::weak_ptr<PromExporter> weakPtr = shared_from_this();
    app.registerHandler(
        path_,
        [weakPtr](const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                auto resp = HttpResponse::newNotFoundResponse();
                callback(resp);
                return;
            }
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(thisPtr->exportMetrics());
            resp->setExpiredTime(5);
            callback(resp);
        },
        {Get, Options},
        "PromExporter");
}
std::string exportCollector(const std::shared_ptr<CollectorBase> &collector)
{
    auto samples = collector->collect();
    std::string res;
    /*
     # HELP process_max_fds Maximum number of open file descriptors.
     # TYPE process_max_fds gauge
     * */
    res.append("# HELP ").append(collector->name()).append(" ").append(collector->help()).append("\n");
    res.append("# TYPE ").append(collector->name()).append(" ").append(collector->type()).append("\n");
    for(auto const &sample : samples)
    {
        res.append(sample.name);
        if(!sample.exLabels.empty()||collector->labelNames().size()>0
        {
            res.append("{");
            for(auto const &label : sample.labels)
            {
                res.append(label.first).append("=\"").append(label.second).append("\",");
            }
            res.pop_back();
            res.append("}");
        }
    }
}
std::string PromExporter::exportMetrics()
{
    std::lock_guard<std::mutex> guard(mutex_);
    std::string result;
    for (auto const &collector : collectors_)
    {
        result.append(exportCollector(collector));
    }
    return result;
}