#include "RangeTestController.h"
// add definition of your processing function here

void RangeTestController::getFile(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    auto resp = HttpResponse::newFileResponse("./range-test.txt");
    callback(resp);
}

void RangeTestController::getFileByRange(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    size_t offset,
    size_t length) const
{
    auto resp =
        HttpResponse::newFileResponse("./range-test.txt", offset, length);
    callback(resp);
}
