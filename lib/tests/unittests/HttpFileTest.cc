#include "../../lib/src/HttpFileImpl.h"
#include <drogon/drogon_test.h>
#include <filesystem>

using namespace drogon;
using namespace std;

DROGON_TEST(HttpFile)
{
    SUBSECTION(Save)
    {
        HttpFileImpl file;
        file.setFileName("test_file_name");
        file.setFile("test", 4);
        auto out = file.save("./test_uploads_dir");

        CHECK(out == 0);
        CHECK(filesystem::exists("./test_uploads_dir/test_file_name"));

        filesystem::remove_all("./test_uploads_dir");
    }

    SUBSECTION(SavePathRelativeTraversal)
    {
        auto uploadPath = filesystem::current_path() / "test_uploads_dir";

        HttpFileImpl file;
        file.setFileName("../test_malicious_file_name");
        file.setFile("test", 4);
        auto out = file.save(uploadPath.string());

        CHECK(out == -1);
        CHECK(!filesystem::exists(uploadPath / "../test_malicious_file_name"));

        filesystem::remove_all(uploadPath);
        filesystem::remove(uploadPath / "../test_malicious_file_name");
    }

    SUBSECTION(SavePathAbsoluteTraversal)
    {
        HttpFileImpl file;
        file.setFileName("/tmp/test_malicious_file_name");
        file.setFile("test", 4);
        auto out = file.save("./test_uploads_dir");

        CHECK(out == -1);
        CHECK(!filesystem::exists("/tmp/test_malicious_file_name"));

        filesystem::remove_all("test_uploads_dir");
        filesystem::remove_all("/tmp/test_malicious_file_name");
    }
}
