#include <drogon/plugins/PromExporter.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/utils/monitoring/Counter.h>
#include <drogon/utils/monitoring/Gauge.h>
#include <drogon/utils/monitoring/Histogram.h>
#include <drogon/utils/monitoring/Collector.h>

using namespace drogon;
using namespace drogon::monitoring;
using namespace drogon::plugin;

void PromExporter::initAndStart(const Json::Value &config)
{
    path_ = config.get("path", path_).asString();
    LOG_TRACE << path_;
    auto &app = drogon::app();
    std::weak_ptr<PromExporter> weakPtr = shared_from_this();
    app.registerHandler(
        path_,
        [weakPtr](const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                auto resp = HttpResponse::newNotFoundResponse(req);
                callback(resp);
                return;
            }
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(thisPtr->exportMetrics());
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->setExpiredTime(5);
            callback(resp);
        },
        {Get, Options},
        "PromExporter");
    if (config.isMember("collectors"))
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto &collectors = config["collectors"];
        if (collectors.isArray())
        {
            for (auto const &collector : collectors)
            {
                if (collector.isObject())
                {
                    auto name = collector["name"].asString();
                    auto type = collector["type"].asString();
                    auto help = collector["help"].asString();
                    auto labels = collector["labels"];
                    if (labels.isArray())
                    {
                        std::vector<std::string> labelNames;
                        for (auto const &label : labels)
                        {
                            if (label.isString())
                            {
                                labelNames.push_back(label.asString());
                            }
                            else
                            {
                                LOG_ERROR << "label name must be a string!";
                            }
                        }
                        if (type == "counter")
                        {
                            auto counterCollector =
                                std::make_shared<Collector<Counter>>(
                                    name, help, labelNames);
                            collectors_.insert(
                                std::make_pair(name, counterCollector));
                        }
                        else if (type == "gauge")
                        {
                            auto gaugeCollector =
                                std::make_shared<Collector<Gauge>>(name,
                                                                   help,
                                                                   labelNames);
                            collectors_.insert(
                                std::make_pair(name, gaugeCollector));
                        }
                        else if (type == "histogram")
                        {
                            auto histogramCollector =
                                std::make_shared<Collector<Histogram>>(
                                    name, help, labelNames);
                            collectors_.insert(
                                std::make_pair(name, histogramCollector));
                        }
                        else
                        {
                            LOG_ERROR << "Unknown collector type: " << type;
                        }
                    }
                    else
                    {
                        LOG_ERROR << "labels must be an array!";
                    }
                }
                else
                {
                    LOG_ERROR << "collector must be an object!";
                }
            }
        }
        else
        {
            LOG_ERROR << "collectors must be an array!";
        }
    }
}

static std::string exportCollector(
    const std::shared_ptr<CollectorBase> &collector)
{
    auto sampleGroups = collector->collect();
    std::string res;
    res.append("# HELP ")
        .append(collector->name())
        .append(" ")
        .append(collector->help())
        .append("\n");
    res.append("# TYPE ")
        .append(collector->name())
        .append(" ")
        .append(collector->type())
        .append("\n");
    for (auto const &sampleGroup : sampleGroups)
    {
        auto const &metricPtr = sampleGroup.metric;
        auto const &samples = sampleGroup.samples;
        for (auto &sample : samples)
        {
            res.append(sample.name);
            if (!sample.exLabels.empty() || !metricPtr->labels().empty())
            {
                res.append("{");
                for (auto const &label : metricPtr->labels())
                {
                    res.append(label.first)
                        .append("=\"")
                        .append(label.second)
                        .append("\",");
                }
                for (auto const &label : sample.exLabels)
                {
                    res.append(label.first)
                        .append("=\"")
                        .append(label.second)
                        .append("\",");
                }
                res.pop_back();
                res.append("}");
            }
            res.append(" ").append(std::to_string(sample.value));
            if (sample.timestamp.microSecondsSinceEpoch() > 0)
            {
                res.append(" ")
                    .append(std::to_string(
                        sample.timestamp.microSecondsSinceEpoch() / 1000))
                    .append("\n");
            }
            else
            {
                res.append("\n");
            }
        }
    }
    return res;
}

std::string PromExporter::exportMetrics()
{
    std::lock_guard<std::mutex> guard(mutex_);
    std::string result;
    for (auto const &collector : collectors_)
    {
        result.append(exportCollector(collector.second));
    }
    return result;
}

void PromExporter::registerCollector(
    const std::shared_ptr<drogon::monitoring::CollectorBase> &collector)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (collectors_.find(collector->name()) != collectors_.end())
    {
        throw std::runtime_error("The collector named " + collector->name() +
                                 " has been registered!");
    }
    collectors_.insert(std::make_pair(collector->name(), collector));
}

std::shared_ptr<drogon::monitoring::CollectorBase> PromExporter::getCollector(
    const std::string &name) const noexcept(false)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto iter = collectors_.find(name);
    if (iter != collectors_.end())
    {
        return iter->second;
    }
    else
    {
        throw std::runtime_error("Can't find the collector named " + name);
    }
}
