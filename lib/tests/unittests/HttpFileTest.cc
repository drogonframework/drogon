#include "../../lib/src/HttpFileImpl.h"
#include <drogon/drogon_test.h>
#include <filesystem>

using namespace drogon;
using namespace std;

DROGON_TEST(HttpFile)
{
    SUBSECTION(SavePathUsingDefaultConfigPath)
    {
        HttpFileImpl file;
        file.setFileName("test_file_name");
        file.setFile("test", 4);
        auto out = file.save();

        CHECK(out == 0);
        CHECK(filesystem::exists("./uploads/test_file_name"));

        filesystem::remove_all("./uploads/test_file_name");
    }

    SUBSECTION(SavePathWithSpecificRelativePath)
    {
        HttpFileImpl file;
        file.setFileName("test_file_name");
        file.setFile("test", 4);
        auto out = file.save("./test_uploads_dir");

        CHECK(out == 0);
        CHECK(filesystem::exists("./test_uploads_dir/test_file_name"));

        filesystem::remove_all("./test_uploads_dir");
    }

    SUBSECTION(SavePathWithSpecificAbsolutePath)
    {
        auto uploadPath = filesystem::current_path() / "test_uploads_dir";

        HttpFileImpl file;
        file.setFileName("test_file_name");
        file.setFile("test", 4);
        auto out = file.save(uploadPath.string());

        CHECK(out == 0);
        CHECK(filesystem::exists(uploadPath / "test_file_name"));

        filesystem::remove_all(uploadPath.string());
    }

    SUBSECTION(FileNameWithRelativePath)
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

    SUBSECTION(FileNameWithAbsolutePath)
    {
        auto fileName = filesystem::current_path() / "test_malicious_file_name";

        HttpFileImpl file;
        file.setFileName(fileName.string());
        file.setFile("test", 4);
        auto out = file.save("./test_uploads_dir");

        CHECK(out == -1);
        CHECK(!filesystem::exists(fileName.string()));

        filesystem::remove_all("test_uploads_dir");
        filesystem::remove(fileName.string());
    }
}
