#pragma once

#include <string>
#include <vector>
#include <memory>
#include <drogon/IOThreadStorage.h>

namespace drogon
{
class HttpFilterBase;

namespace internal
{

struct CtrlBinderBase
{
    std::string handlerName_;
    std::vector<std::string> filterNames_;
    std::vector<std::shared_ptr<HttpFilterBase>> filters_;
    IOThreadStorage<HttpResponsePtr> responseCache_;
    std::shared_ptr<std::string> corsMethods_;
    bool isCORS_{false};

    bool isSimple_{false};
};

struct RouteResult
{
    bool found;
    std::shared_ptr<CtrlBinderBase> binderPtr;
};

}  // namespace internal
}  // namespace drogon
