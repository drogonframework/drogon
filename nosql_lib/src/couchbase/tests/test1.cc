#include <drogon/nosql/CouchBaseClient.h>
#include <trantor/utils/Logger.h>
using namespace drogon::nosql;
int main()
{
    auto client = drogon::nosql::CouchBaseClient::newClient(
        "couchbase://127.0.0.1", "root", "123456", "beer-sample");
    client->get(
        "21st_amendment_brewery_cafe",
        [](const CouchBaseResult &r) { LOG_INFO << "value: " << r.getValue(); },
        [](const NosqlException &err) {});
    getchar();
}