#include <drogon/orm/PgClient.h>
#include <drogon/orm/Mapper.h>

#include <trantor/utils/Logger.h>
#include <iostream>
#include <unistd.h>
#include <string>

using namespace drogon::orm;
class User
{
  public:
    const static std::string primaryKeyName;
    const static bool hasPrimaryKey;
    const static std::string tableName;

    typedef int PrimaryKeyType;
    User(const Row &r)
        : _userId(r["user_id"].as<std::string>()),
          _userName(r["user_name"].as<std::string>())
    {
    }
    std::string _userId;
    std::string _userName;
};
const std::string User::primaryKeyName = "user_uuid";
const bool User::hasPrimaryKey = true;
const std::string User::tableName = "users";

int main()
{
    drogon::orm::PgClient client("host=127.0.0.1 port=5432 dbname=trantor user=antao", 1);
    LOG_DEBUG << "start!";
    sleep(1);
    //client << "\\d users" >> [](const Result &r) {} >> [](const DrogonDbException &e) { std::cerr << e.base().what() << std::endl; };
    Mapper<User> mapper(client);
    auto U = mapper.findByPrimaryKey(2);
    std::cout << "id=" << U._userId << std::endl;
    std::cout << "name=" << U._userName << std::endl;
    getchar();
}