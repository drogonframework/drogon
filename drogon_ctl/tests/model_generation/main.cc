#include "AutoKey.h"
#include "CompositeKey.h"
#include "ManualKey.h"
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Mapper.h>
#include <iostream>
#include <tuple>

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    using namespace drogon_model::sqlite3;
    using drogon::orm::Mapper;
    try
    {
        std::string error;
        const Json::Value id(Json::Int64(42));
        if (AutoKey::validJsonOfField(0, "id", id, error, true) ||
            !AutoKey::validJsonOfField(0, "id", id, error, false) ||
            !ManualKey::validJsonOfField(0, "id", id, error, true) ||
            !ManualKey::validJsonOfField(0, "id", id, error, false))
            return 6;
        auto client = drogon::orm::DbClient::newSqlite3Client(
            std::string("filename=") + argv[1], 1);
        client->execSqlSync("DELETE FROM auto_key");
        client->execSqlSync("DELETE FROM manual_key");
        client->execSqlSync("DELETE FROM composite_key");
        AutoKey first;
        first.setValue("first");
        Mapper<AutoKey>(client).insert(first);
        AutoKey second;
        second.setValue("second");
        Mapper<AutoKey>(client).insert(second);
        if (first.getPrimaryKey() <= 0 ||
            second.getPrimaryKey() <= first.getPrimaryKey())
            return 2;
        ManualKey manual;
        manual.setId(42);
        manual.setValue("manual");
        Mapper<ManualKey>(client).insert(manual);
        if (manual.getPrimaryKey() != 42)
            return 3;
        CompositeKey composite;
        composite.setA("a");
        composite.setB("b");
        composite.setValue("composite");
        Mapper<CompositeKey>(client).insert(composite);
        if (composite.getPrimaryKey() !=
            std::make_tuple(std::string("a"), std::string("b")))
            return 4;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 5;
    }
}
