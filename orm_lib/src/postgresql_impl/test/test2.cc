#include <drogon/orm/DbClient.h>
#include <drogon/orm/Mapper.h>

#include <iostream>
#include <string>
#include <trantor/utils/Logger.h>
#include <unistd.h>

using namespace drogon::orm;
class User
{
  public:
    const static std::string primaryKeyName;
    const static bool hasPrimaryKey;
    const static std::string tableName;

    typedef int PrimaryKeyType;
    explicit User(const Row &r)
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
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    auto client =
        DbClient::newPgClient("host=127.0.0.1 port=5432 dbname=test user=antao",
                              1);
    sleep(1);
    LOG_DEBUG << "start!";
    {
        auto trans = client->newTransaction([](bool committed) {
            std::cout << "The transaction submission "
                      << (committed ? "succeeded" : "failed") << std::endl;
        });
        *trans << "delete from users where user_uuid=201" >>
            [trans](const Result &r) {
                std::cout << "delete " << r.affectedRows() << "user!!!!!"
                          << std::endl;
                trans->rollback();
            } >>
            [](const DrogonDbException &e) {
                std::cout << e.base().what() << std::endl;
            };

        *trans << "delete from users where user_uuid=201" >>
            [](const Result &r) {
                std::cout << "delete " << r.affectedRows() << "user!!!!!"
                          << std::endl;
            } >>
            [](const DrogonDbException &e) {
                std::cout << e.base().what() << std::endl;
            };
    }

    Mapper<User> mapper(client);
    auto U = mapper.findByPrimaryKey(2);
    std::cout << "id=" << U._userId << std::endl;
    std::cout << "name=" << U._userName << std::endl;
    *client << "select * from array_test" >>
        [=](bool isNull,
            const std::vector<std::shared_ptr<int>> &a,
            const std::string &b,
            int c) {
            if (!isNull)
            {
                std::cout << "a.len=" << a.size() << std::endl;
                for (size_t i = 0; i < a.size(); i++)
                {
                    std::cout << "a[" << i << "]=" << *a[i] << " ";
                }
                std::cout << std::endl;
                std::cout << "b=" << b << " b.len=" << b.length() << std::endl;
                std::cout << "c=" << c << std::endl;
            }
        } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    for (int i = 0; i < 100; i++)
        mapper.findByPrimaryKey(
            2,
            [](User u) { std::cout << "get a user by pk" << std::endl; },
            [](const DrogonDbException &e) { throw std::exception(); });
    getchar();
}
