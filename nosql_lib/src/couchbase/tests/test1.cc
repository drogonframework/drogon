#include <drogon/nosql/CouchBaseClient.h>
#include <trantor/utils/Logger.h>
#include <thread>
using namespace drogon::nosql;
int main()
{
    auto client = drogon::nosql::CouchBaseClient::newClient(
        "couchbase://127.0.0.1/beer-sample", "root", "123456");
    client->store(
        "hello",
        "world",
        [](const CouchBaseResult &r) { LOG_INFO << "store successfully! "; },
        [](const NosqlException &err) {});
    client->get(
        "21st_amendment_brewery_cafe",
        [](const CouchBaseResult &r) { LOG_INFO << "value: " << r.getValue(); },
        [](const NosqlException &err) {});
    client->get(
        "hello",
        [](const CouchBaseResult &r) { LOG_INFO << "value: " << r.getValue(); },
        [](const NosqlException &err) {});
    std::thread([client]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        client->get(
            "21st_amendment_brewery_cafe",
            [](const CouchBaseResult &r) {
                LOG_INFO << "value: " << r.getValue();
            },
            [](const NosqlException &err) {});
    }).detach();
    client->get(
        "xxxxnotfoundxxxx1",
        [](const CouchBaseResult &r) { LOG_INFO << "value: " << r.getValue(); },
        [](const NosqlException &err) {
            LOG_ERROR << "can't found value by key:" << err.what();
        });
    getchar();
}