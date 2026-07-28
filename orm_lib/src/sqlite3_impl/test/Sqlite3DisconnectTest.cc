#include <drogon/orm/DbClient.h>
#include <sqlite3.h>

#include <iostream>
#include <limits>
#include <stdexcept>

using drogon::orm::DbClient;

namespace
{
void runClient(bool cachePreparedStatement)
{
    auto client = DbClient::newSqlite3Client("filename=:memory:", 1);
    if (cachePreparedStatement)
    {
        const auto result = client->execSqlSync("SELECT ?", 42);
        if (result.size() != 1 || result[0][0].as<int>() != 42)
        {
            throw std::runtime_error("unexpected SQLite query result");
        }
    }
    else
    {
        client->execSqlSync("SELECT 1");
    }
    client->closeAll();
}
}  // namespace

int main()
{
    // Initialize SQLite and Drogon's database machinery before taking the
    // baseline. A query without parameters does not enter the statement cache.
    runClient(false);
    sqlite3_release_memory(std::numeric_limits<int>::max());
    const auto memoryBefore = sqlite3_memory_used();

    runClient(true);
    sqlite3_release_memory(std::numeric_limits<int>::max());
    const auto memoryAfter = sqlite3_memory_used();

    if (memoryAfter > memoryBefore)
    {
        std::cerr << "SQLite retained " << memoryAfter - memoryBefore
                  << " bytes after the client disconnected\n";
        return 1;
    }
}
