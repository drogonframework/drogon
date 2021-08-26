#include "RangeTestController.h"

#include <fstream>

size_t RangeTestController::fileSize_ = 10000 * 100;  // 1e6 Bytes

RangeTestController::RangeTestController()
{
    std::ofstream outfile("./range-test.txt", std::ios::out | std::ios::trunc);
    for (int i = 0; i < 10000; ++i)
    {
        outfile.write(
            "01234567890123456789"
            "01234567890123456789"
            "01234567890123456789"
            "01234567890123456789"
            "01234567890123456789",
            100);
    }
}

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
