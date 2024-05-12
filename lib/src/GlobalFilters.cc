#include <drogon/plugins/GlobalFilters.h>
#include <drogon/DrClassMap.h>
#include <drogon/HttpAppFramework.h>
#include "MiddlewaresFunction.h"
#include "HttpRequestImpl.h"
#include "HttpAppFrameworkImpl.h"

using namespace drogon::plugin;

void GlobalFilters::initAndStart(const Json::Value &config)
{
    if (config.isMember("filters") && config["filters"].isArray())
    {
        auto &filters = config["filters"];

        for (auto const &filter : filters)
        {
            if (filter.isString())
            {
                auto filterPtr = std::dynamic_pointer_cast<HttpFilterBase>(
                    drogon::DrClassMap::getSingleInstance(filter.asString()));
                if (filterPtr)
                {
                    filters_.push_back(filterPtr);
                }
                else
                {
                    LOG_ERROR << "Filter " << filter.asString()
                              << " not found!";
                }
            }
        }
    }
    if (config.isMember("exempt"))
    {
        auto exempt = config["exempt"];
        if (exempt.isArray())
        {
            std::string regexStr;
            for (auto const &ex : exempt)
            {
                if (ex.isString())
                {
                    regexStr.append("(").append(ex.asString()).append(")|");
                }
                else
                {
                    LOG_ERROR << "exempt must be a string array!";
                }
            }
            if (!regexStr.empty())
            {
                regexStr.pop_back();
                exemptPegex_ = std::regex(regexStr);
                regexFlag_ = true;
            }
        }
        else if (exempt.isString())
        {
            exemptPegex_ = std::regex(exempt.asString());
            regexFlag_ = true;
        }
        else
        {
            LOG_ERROR << "exempt must be a string or string array!";
        }
    }
    std::weak_ptr<GlobalFilters> weakPtr = shared_from_this();
    drogon::app().registerPreRoutingAdvice(
        [weakPtr](const drogon::HttpRequestPtr &req,
                  drogon::AdviceCallback &&acb,
                  drogon::AdviceChainCallback &&accb) {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                accb();
                return;
            }
            if (thisPtr->regexFlag_)
            {
                if (std::regex_match(req->path(), thisPtr->exemptPegex_))
                {
                    accb();
                    return;
                }
            }

            drogon::middlewares_function::doFilters(
                thisPtr->filters_,
                std::static_pointer_cast<HttpRequestImpl>(req),
                [acb = std::move(acb),
                 accb = std::move(accb)](const HttpResponsePtr &resp) {
                    if (resp)
                    {
                        acb(resp);
                    }
                    else
                    {
                        accb();
                    }
                });
        });
}

void GlobalFilters::shutdown()
{
    filters_.clear();
}
