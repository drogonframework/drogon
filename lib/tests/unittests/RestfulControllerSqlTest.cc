#include <drogon/drogon_test.h>
#include <drogon/orm/RestfulController.h>

using namespace drogon;

namespace
{
class TestRestfulController : public RestfulController
{
  public:
    TestRestfulController() : RestfulController({"id", "secret"})
    {
        enableMasquerading({"public_id", ""});
    }

    using RestfulController::columnIndexForField;
};

Json::Value filter(const std::string &field,
                   const std::string &op,
                   const Json::Value &value)
{
    Json::Value condition(Json::arrayValue);
    condition.append(field);
    condition.append(op);
    condition.append(value);
    Json::Value conjunction(Json::arrayValue);
    conjunction.append(condition);
    Json::Value disjunction(Json::arrayValue);
    disjunction.append(conjunction);
    return disjunction;
}
}  // namespace

DROGON_TEST(RestfulControllerRejectsUnmappedFieldsAndOperators)
{
    TestRestfulController controller;
    CHECK(controller.columnIndexForField("public_id").value() == 0);
    CHECK(!controller.columnIndexForField("secret"));
    CHECK(!controller.columnIndexForField("id desc; drop table users"));
    CHECK(controller.makeCriteria(filter("public_id", "=", "42"))
              .criteriaString() == "id = $?");

    CHECK_THROWS_AS(controller.makeCriteria(
                        filter("public_id", " = 1 OR 1=1 OR id =", "42")),
                    std::runtime_error);
    CHECK_THROWS_AS(controller.makeCriteria(filter("id OR 1=1", "=", "42")),
                    std::runtime_error);
    CHECK_THROWS_AS(controller.makeCriteria(filter("secret", "=", "42")),
                    std::runtime_error);

    controller.disableMasquerading();
    CHECK(controller.columnIndexForField("secret").value() == 1);
    CHECK(!controller.columnIndexForField("public_id"));
    CHECK(controller.makeCriteria(filter("id", "=", "42")).criteriaString() ==
          "id = $?");

    controller.enableMasquerading({"new_id", ""});
    CHECK(!controller.columnIndexForField("public_id"));
    CHECK(controller.columnIndexForField("new_id").value() == 0);
}

DROGON_TEST(JsonCriteriaOnlyUsesKnownOperators)
{
    CHECK(orm::Criteria(filter("users.id", "=", "42")[0][0]).criteriaString() ==
          "users.id = $?");
    CHECK(orm::Criteria(filter("id", "=", "' OR 1=1 --")[0][0])
              .criteriaString() == "id = $?");
    CHECK(
        orm::Criteria(filter("id", " LIKE ", "alice")[0][0]).criteriaString() ==
        "id like $?");
    CHECK(orm::Criteria(filter("id", "!=", Json::Value())[0][0])
              .criteriaString() == "id is not null");
    CHECK(orm::Criteria(filter("id", " != ", Json::Value())[0][0])
              .criteriaString() == "id is not null");
    Json::Value values(Json::arrayValue);
    values.append("1");
    values.append("2");
    CHECK(orm::Criteria(filter("id", "in", values)[0][0]).criteriaString() ==
          "id in ($?,$?)");
    CHECK(orm::Criteria(filter("id", " IN ", values)[0][0]).criteriaString() ==
          "id in ($?,$?)");
    CHECK_THROWS_AS(orm::Criteria(
                        filter("id", "= 1 OR 1=1 OR id =", "42")[0][0]),
                    std::runtime_error);
}
