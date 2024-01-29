#pragma once
#include <drogon/HttpController.h>
using namespace drogon;

class RangeTestController : public drogon::HttpController<RangeTestController>
{
  public:
    METHOD_LIST_BEGIN
    // path is /RangeTestController
    METHOD_ADD(RangeTestController::getFile, "/", Get);
    // path is /RangeTestController/{offset}/{length}
    METHOD_ADD(RangeTestController::getFileByRange, "/{offset}/{length}", Get);
    METHOD_LIST_END

    RangeTestController();

    void getFile(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback) const;

    // We do not provide 'Range' header decoding, simply use path as range
    // parameter.
    void getFileByRange(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        size_t offset,
                        size_t length) const;

    static size_t getFileSize()
    {
        return fileSize_;
    }

  private:
    static size_t fileSize_;
};
