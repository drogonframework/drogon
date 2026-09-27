#include <drogon/drogon_test.h>
#include <drogon/orm/DbClient.h>
#include <drogon/utils/Utilities.h>
#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>

using namespace std::chrono_literals;
using drogon::orm::BrokenConnection;
using drogon::orm::DbClient;

namespace
{
std::atomic<int> connectionsOpened{0};
std::atomic<int> rollbackAttempts{0};

struct FaultState
{
    int connectionId{++connectionsOpened};
    bool denyRollback{false};
    bool denyCommit{false};
    bool rejectCommit{false};
};

int authorize(void *context,
              int action,
              const char *operation,
              const char *,
              const char *,
              const char *)
{
    auto &state = *static_cast<FaultState *>(context);
    if (action == SQLITE_TRANSACTION && operation &&
        std::string(operation) == "COMMIT" && state.denyCommit)
        return SQLITE_DENY;
    if (action == SQLITE_TRANSACTION && operation &&
        std::string(operation) == "ROLLBACK")
    {
        ++rollbackAttempts;
        if (state.denyRollback)
            return SQLITE_DENY;
    }
    return SQLITE_OK;
}

int commitHook(void *context)
{
    auto &state = *static_cast<FaultState *>(context);
    const bool reject = state.rejectCommit;
    state.rejectCommit = false;
    return reject ? 1 : 0;
}

void setFault(sqlite3_context *context, int, sqlite3_value **arguments)
{
    auto &state = *static_cast<FaultState *>(sqlite3_user_data(context));
    const auto value = sqlite3_value_text(arguments[0]);
    const std::string mode = value ? reinterpret_cast<const char *>(value) : "";
    if (mode == "deny_rollback")
        state.denyRollback = true;
    if (mode == "deny_commit_and_rollback")
    {
        state.denyCommit = true;
        state.denyRollback = true;
    }
    if (mode == "automatic_rollback")
        state.rejectCommit = true;
    sqlite3_result_int(context, state.connectionId);
}

int installFaults(sqlite3 *db, char **, const sqlite3_api_routines *)
{
    auto state = new FaultState;
    const int result =
        sqlite3_create_function_v2(db,
                                   "test_fault",
                                   1,
                                   SQLITE_UTF8,
                                   state,
                                   setFault,
                                   nullptr,
                                   nullptr,
                                   [](void *value) {
                                       delete static_cast<FaultState *>(value);
                                   });
    if (result != SQLITE_OK)
        return result;
    sqlite3_commit_hook(db, commitHook, state);
    return sqlite3_set_authorizer(db, authorize, state);
}

struct Fixture
{
    std::filesystem::path path;
    drogon::orm::DbClientPtr client;

    explicit Fixture(bool inMemory = false, size_t connectionCount = 1)
    {
        if (!inMemory)
            path = std::filesystem::temp_directory_path() /
                   ("drogon-commit-" + drogon::utils::getUuid() + ".db");
        const auto filename = inMemory ? ":memory:" : path.string();
        client = DbClient::newSqlite3Client("filename='" + filename + "'",
                                            connectionCount);
        client->execSqlSync("PRAGMA foreign_keys = ON");
        client->execSqlSync("CREATE TABLE parents(id INTEGER PRIMARY KEY)");
        client->execSqlSync(
            "CREATE TABLE children(parent_id INTEGER REFERENCES "
            "parents(id) DEFERRABLE INITIALLY DEFERRED)");
        client->execSqlSync("INSERT INTO parents VALUES(100)");
    }

    ~Fixture()
    {
        client.reset();
        if (!path.empty())
        {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    }
};

bool brokenConnection(std::future<drogon::orm::Result> &result)
{
    try
    {
        result.get();
    }
    catch (const BrokenConnection &)
    {
        return true;
    }
    return false;
}
}  // namespace

DROGON_TEST(SQLiteFailedCommitRecovery)
{
    for (const std::string mode :
         {"rollback", "deny_rollback", "automatic_rollback"})
    {
        for (bool withCallback : {true, false})
        {
            // Automatic rollback must preserve an in-memory database, not
            // discard its already-clean connection.
            Fixture fixture(mode == "automatic_rollback");
            auto &client = fixture.client;
            // Exercise both timeout wrappers and the default, no-timeout path.
            // Failure must not depend on the configured timeout expiring.
            if (withCallback)
                client->setTimeout(30.0);
            const auto identity = client->execSqlSync("SELECT test_fault('')");
            const int originalConnection = identity[0][0].as<int>();
            const int previousConnections = connectionsOpened.load();
            const int previousRollbacks = rollbackAttempts.load();
            auto callbacks = std::make_shared<std::atomic<int>>(0);
            auto completed = std::make_shared<std::promise<bool>>();
            auto result = completed->get_future();
            auto transaction = client->newTransaction();
            if (withCallback)
            {
                transaction->setCommitCallback([completed, callbacks](bool ok) {
                    if (++*callbacks == 1)
                        completed->set_value(ok);
                });
            }
            // Parameterized SQL also exercises cleanup of cached statements.
            transaction->execSqlSync("INSERT INTO parents VALUES(?)", 1);
            if (mode != "automatic_rollback")
                transaction->execSqlSync("INSERT INTO children VALUES(?)", 2);
            transaction->execSqlSync("SELECT test_fault(?)", mode);
            auto queued = client->execSqlAsyncFuture(
                "SELECT (SELECT count(*) FROM parents),"
                " (SELECT count(*) FROM children), test_fault('')");
            auto queuedTransaction = std::make_shared<
                std::promise<std::shared_ptr<drogon::orm::Transaction>>>();
            auto queuedTransactionResult = queuedTransaction->get_future();
            if (mode == "deny_rollback")
                client->newTransactionAsync(
                    [queuedTransaction](const auto &tx) {
                        queuedTransaction->set_value(tx);
                    });
            transaction.reset();
            if (withCallback)
            {
                REQUIRE(result.wait_for(5s) == std::future_status::ready);
                CHECK(!result.get());
            }
            REQUIRE(queued.wait_for(5s) == std::future_status::ready);
            CHECK(rollbackAttempts - previousRollbacks ==
                  (mode == "automatic_rollback" ? 0 : 1));
            CHECK(callbacks->load() == (withCallback ? 1 : 0));
            if (mode == "deny_rollback")
            {
                CHECK(brokenConnection(queued));
                REQUIRE(queuedTransactionResult.wait_for(5s) ==
                        std::future_status::ready);
                CHECK(!queuedTransactionResult.get());
                CHECK(!client->hasAvailableConnections());

                auto subsequent = client->execSqlAsyncFuture("SELECT 1");
                REQUIRE(subsequent.wait_for(5s) == std::future_status::ready);
                CHECK(brokenConnection(subsequent));
                bool rejected = false;
                try
                {
                    client->newTransaction();
                }
                catch (const BrokenConnection &)
                {
                    rejected = true;
                }
                CHECK(rejected);
                CHECK(connectionsOpened.load() == previousConnections);

                // Closing the retired handle must release the transaction and
                // its locks, even while the failed client is still alive.
                auto reopened = DbClient::newSqlite3Client(
                    "filename='" + fixture.path.string() + "'", 1);
                const auto saved = reopened->execSqlSync(
                    "SELECT (SELECT count(*) FROM parents),"
                    " (SELECT count(*) FROM children)");
                CHECK(saved[0][0].as<int>() == 1);
                CHECK(saved[0][1].as<int>() == 0);
                reopened->execSqlSync("INSERT INTO parents VALUES(200)");
                continue;
            }
            const auto rows = queued.get();
            CHECK(rows[0][0].as<int>() == 1);
            CHECK(rows[0][1].as<int>() == 0);
            CHECK(rows[0][2].as<int>() == originalConnection);
            const auto foreignKeys = client->execSqlSync("PRAGMA foreign_keys");
            CHECK(foreignKeys[0][0].as<int>() == 1);

            // The pool must still support a subsequent successful transaction.
            auto committed = std::make_shared<std::promise<bool>>();
            auto committedResult = committed->get_future();
            transaction = client->newTransaction(
                [committed](bool ok) { committed->set_value(ok); });
            transaction->execSqlSync("INSERT INTO parents VALUES(?)", 2);
            transaction->execSqlSync("INSERT INTO children VALUES(?)", 2);
            transaction.reset();
            REQUIRE(committedResult.wait_for(5s) == std::future_status::ready);
            CHECK(committedResult.get());
            const auto saved =
                client->execSqlSync("SELECT parent_id FROM children");
            REQUIRE(saved.size() == 1);
            CHECK(saved[0][0].as<int>() == 2);
        }
    }
}

DROGON_TEST(SQLiteFailedCommitRemainingConnection)
{
    Fixture fixture(false, 2);
    auto &client = fixture.client;
    auto completed = std::make_shared<std::promise<bool>>();
    auto result = completed->get_future();
    auto failed = client->newTransaction(
        [completed](bool ok) { completed->set_value(ok); });
    auto healthy = client->newTransaction();
    healthy->execSqlSync("SELECT 1");
    const int previousConnections = connectionsOpened.load();
    failed->execSqlSync("INSERT INTO parents VALUES(?)", 1);
    failed->execSqlSync("SELECT test_fault('deny_commit_and_rollback')");
    auto queued = client->execSqlAsyncFuture("SELECT count(*) FROM parents");
    failed.reset();
    REQUIRE(result.wait_for(5s) == std::future_status::ready);
    CHECK(!result.get());
    CHECK(client->hasAvailableConnections());
    // The surviving connection can write after the failed handle is closed.
    healthy->execSqlSync("INSERT INTO parents VALUES(?)", 2);
    healthy.reset();
    REQUIRE(queued.wait_for(5s) == std::future_status::ready);
    CHECK(queued.get()[0][0].as<int>() == 2);

    // Losing the last healthy connection must now fail queued work promptly.
    failed = client->newTransaction();
    failed->execSqlSync("INSERT INTO parents VALUES(?)", 3);
    failed->execSqlSync("SELECT test_fault('deny_commit_and_rollback')");
    queued = client->execSqlAsyncFuture("SELECT count(*) FROM parents");
    failed.reset();
    REQUIRE(queued.wait_for(5s) == std::future_status::ready);
    CHECK(brokenConnection(queued));
    CHECK(!client->hasAvailableConnections());
    CHECK(connectionsOpened.load() == previousConnections);
}

DROGON_TEST(SQLiteFailedCommitShutdown)
{
    Fixture fixture;
    auto completed = std::make_shared<std::promise<bool>>();
    auto result = completed->get_future();
    auto transaction = fixture.client->newTransaction(
        [completed](bool ok) { completed->set_value(ok); });
    transaction->execSqlSync("INSERT INTO children VALUES(?)", 2);
    transaction->execSqlSync("SELECT test_fault('deny_rollback')");
    transaction.reset();
    REQUIRE(result.wait_for(5s) == std::future_status::ready);
    CHECK(!result.get());
    // Retired connections must be destroyed without joining their own thread.
    fixture.client.reset();
}

int main(int argc, char **argv)
{
    // Initialize SQLite through Drogon before registering the test-only
    // extension. Keep its process-wide hooks out of the other database tests.
    {
        auto client = DbClient::newSqlite3Client("filename=:memory:", 1);
        client->execSqlSync("SELECT 1");
    }
    const auto entry = reinterpret_cast<void (*)()>(installFaults);
    if (sqlite3_auto_extension(entry) != SQLITE_OK)
    {
        std::cerr << "Could not install SQLite fault injection\n";
        return 1;
    }
    const int result = drogon::test::run(argc, argv);
    sqlite3_cancel_auto_extension(entry);
    return result;
}
