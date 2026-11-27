#include <drogon/drogon_test.h>
#include <drogon/orm/DbClient.h>
#include <drogon/utils/Utilities.h>

#include <filesystem>
#include <string>

#include "ConnectionStringHelper.h"

using drogon::orm::DbClient;

namespace
{
// UTF-8 bytes of a non-ASCII directory name, spelled out so that the test does
// not depend on the encoding of this source file.
const std::string kNonAscii{std::string("\xE8\xB7\xAF\xE5\xBE\x84")};

std::filesystem::path tmpDirectory()
{
    // A space and a single quote both have a meaning inside a connection
    // string, so a path containing them has to be quoted and escaped.
    const std::string name =
        "drogon " + kNonAscii + " O'Connor " + drogon::utils::getUuid();
    return std::filesystem::temp_directory_path() /
           std::filesystem::path(drogon::utils::toNativePath(name));
}
}  // namespace

DROGON_TEST(SQLiteConnectionStringEscaping)
{
    using drogon::test::sqlite3::quoteConnectionValue;

    const auto directory = tmpDirectory();
    const auto filename = directory / "test.db";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    REQUIRE(std::filesystem::create_directories(directory, error));

    auto client =
        DbClient::newSqlite3Client("filename=" + quoteConnectionValue(filename),
                                   1);
    client->execSqlSync("CREATE TABLE t(id INTEGER PRIMARY KEY)");
    client->execSqlSync("INSERT INTO t VALUES(1)");
    CHECK(client->execSqlSync("SELECT count(*) FROM t")[0][0].as<int>() == 1);

    // The database must be created where the path says. A connection string
    // that loses its backslashes ends up with a drive-relative path on Windows
    // and silently writes the file somewhere else.
    REQUIRE(std::filesystem::exists(filename));

    // SQLite reports the file it actually opened, so this also catches a path
    // that was rewritten while being parsed. The two paths are compared by
    // asking the filesystem whether they are the same file, because SQLite
    // resolves the directory (on macOS /var is reached through /private/var)
    // and the textual form can legitimately differ per platform.
    const auto opened = client->execSqlSync("PRAGMA database_list");
    REQUIRE(opened.size() == 1);
    const auto openedPath = std::filesystem::path(
        drogon::utils::toNativePath(opened[0][2].as<std::string>()));
    std::error_code sameFileError;
    CHECK(std::filesystem::equivalent(openedPath, filename, sameFileError));

    client.reset();
    std::filesystem::remove_all(directory, error);
    CHECK(!std::filesystem::exists(filename));
}

int main(int argc, char **argv)
{
    return drogon::test::run(argc, argv);
}
