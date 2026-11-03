#include "../../lib/src/HttpFileImpl.h"
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <filesystem>
#include <fstream>
#if !defined(_WIN32)
#include <unistd.h>
#endif

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

    SUBSECTION(FileNameWithStrictPrefixTraversal)
    {
        // Upload path with multiple subdirectories so that destination is a
        // strict ancestor, making fsSaveToPath have fewer components while
        // matching all initial components of fsUploadDir.
        auto uploadPath = filesystem::current_path() / "test_uploads_dir" /
                          "a" / "b" / "c" / "upload";

        HttpFileImpl file;
        file.setFileName("../../c");
        file.setFile("test", 4);
        auto out = file.save(uploadPath.string());

        CHECK(out == -1);

        filesystem::remove_all(filesystem::current_path() / "test_uploads_dir");
    }

    SUBSECTION(FileNameWithDotDotTraversal)
    {
        auto uploadPath = filesystem::current_path() / "test_uploads_dir";

        HttpFileImpl file;
        file.setFileName("..");
        file.setFile("test", 4);
        auto out = file.save(uploadPath.string());

        CHECK(out == -1);

        filesystem::remove_all(uploadPath);
    }

    SUBSECTION(FileNameWithNestedSubdirectory)
    {
        auto uploadPath = filesystem::current_path() / "test_uploads_dir";
        filesystem::create_directories(uploadPath / "nested");

        HttpFileImpl file;
        file.setFileName("nested/test_nested_file");
        file.setFile("test", 4);
        auto out = file.save(uploadPath.string());

        CHECK(out == 0);
        CHECK(filesystem::exists(uploadPath / "nested" / "test_nested_file"));

        filesystem::remove_all(uploadPath);
    }
}

DROGON_TEST(HttpFileSaveRejectsParentDirectoryName)
{
    // weakly_canonical() makes the target shorter than the upload directory
    // here; the prefix check must not read past the end of the target path.
    auto uploadPath = filesystem::current_path() / "test_uploads_dir";
    for (const auto *name : {"..", "a/../.."})
    {
        HttpFileImpl file;
        file.setFileName(name);
        file.setFile("test", 4);
        CHECK(file.save(uploadPath.string()) == -1);
    }
    filesystem::remove_all(uploadPath);
}

static size_t getPathComponentLimit(const filesystem::path &path)
{
#if defined(_PC_NAME_MAX) && !defined(_WIN32)
    long limit = ::pathconf(path.string().c_str(), _PC_NAME_MAX);
    if (limit > 0)
        return static_cast<size_t>(limit);
#endif
#if defined(NAME_MAX)
    return NAME_MAX;
#else
    return 255;
#endif
}

DROGON_TEST(HttpFileExceptionSafety)
{
    auto uploadPath = filesystem::current_path() / "test_uploads_dir";
    const size_t nameLimit = getPathComponentLimit(filesystem::current_path());

    SUBSECTION(FilenameExceedingNameMax)
    {
        // Filename exceeding filesystem NAME_MAX (typically 255 bytes).
        // weakly_canonical() must not throw std::filesystem_error or terminate
        // the process.
        std::string longFileName(nameLimit + 50, 'x');
        HttpFileImpl file;
        file.setFileName(longFileName);
        file.setFile("test", 4);
        CHECK(file.save(uploadPath.string()) == -1);
    }

    SUBSECTION(SaveAsWithExcessiveParentDirectory)
    {
        // Excessive component in parent_path exercises exists(parent, ec)
        std::string longDirName(nameLimit + 50, 'y');
        HttpFileImpl file;
        file.setFileName("normal.txt");
        file.setFile("test", 4);
        const auto target = uploadPath / longDirName / "upload.bin";
        CHECK(file.saveAs(target.string()) == -1);
    }

    SUBSECTION(SavePathWithNormalRelativePath)
    {
        // Normal relative paths (not starting with "." or "..") must resolve
        // against the configured upload directory (app().getUploadPath()).
        struct UploadPathGuard
        {
            std::string originalPath_{drogon::app().getUploadPath()};

            ~UploadPathGuard()
            {
                // setUploadPath() does not accept an empty path.
                // Restore the default upload directory if no original path
                // exists.
                drogon::app().setUploadPath(
                    originalPath_.empty() ? "uploads" : originalPath_);
            }
        } guard;

        auto customUploadDir =
            filesystem::current_path() / "test_custom_uploads";
        filesystem::create_directories(customUploadDir);
        drogon::app().setUploadPath(customUploadDir.string());

        HttpFileImpl file;
        file.setFileName("normal_rel.txt");
        file.setFile("test", 4);
        int res = file.save("custom_sub");
        CHECK(res == 0);
        auto expectedPath = customUploadDir / "custom_sub" / "normal_rel.txt";
        CHECK(filesystem::exists(expectedPath));

        filesystem::remove_all(customUploadDir);
    }

    SUBSECTION(RelativePathCurrentPathBranch)
    {
        // Path beginning with "." or ".." takes the current_path() branch
        HttpFileImpl file;
        file.setFileName("test_rel.txt");
        file.setFile("test", 4);
        int res = file.save("./test_uploads_dir/rel_subdir");
        CHECK(res == 0);
        CHECK(filesystem::exists(uploadPath / "rel_subdir" / "test_rel.txt"));
        filesystem::remove_all(uploadPath);
    }

    SUBSECTION(SaveAsInaccessibleParentDirectory)
    {
        // Uses a destination whose parent path traverses through an existing
        // regular file. Verifies that the resulting ENOTDIR filesystem error is
        // handled gracefully and saveAs() returns -1.
        HttpFileImpl file;
        file.setFileName("test.txt");
        file.setFile("test", 4);
        auto blocker = filesystem::current_path() / "test_blocker_file.tmp";
        {
            std::ofstream f(blocker);
            REQUIRE(f.is_open());
            f.put('x');
        }
        const auto target = blocker / "child" / "upload.bin";
        int res = file.saveAs(target.string());
        CHECK(res == -1);
        filesystem::remove(blocker);
    }

    SUBSECTION(UploadPathIsRegularFile)
    {
        // Passing an existing regular file as upload directory fails gracefully
        auto dummyFile = filesystem::current_path() / "test_file_dir.tmp";
        {
            std::ofstream f(dummyFile);
            REQUIRE(f.is_open());
            f << "dummy";
        }
        HttpFileImpl file;
        file.setFileName("out.txt");
        file.setFile("test", 4);
        CHECK(file.save(dummyFile.string()) == -1);
        filesystem::remove(dummyFile);
    }
}
