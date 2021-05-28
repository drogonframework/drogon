#include <drogon/DrObject.h>
#include <drogon/drogon_test.h>

using namespace drogon;

class TestA : public DrObject<TestA>
{
};
namespace test
{
class TestB : public DrObject<TestB>
{
};
}  // namespace test

DROGON_TEST(DrObjectCreationTest)
{
    using PtrType = std::shared_ptr<DrObjectBase>;
    auto obj = PtrType(DrClassMap::newObject("TestA"));
    CHECK(obj != nullptr);

    auto objPtr = DrClassMap::getSingleInstance("TestA");
    CHECK(objPtr.get() != nullptr);

    auto objPtr2 = DrClassMap::getSingleInstance<TestA>();
    CHECK(objPtr2.get() != nullptr);
    CHECK(objPtr == objPtr2);
}

DROGON_TEST(DrObjectNamespaceTest)
{
    using PtrType = std::shared_ptr<DrObjectBase>;
    auto obj = PtrType(DrClassMap::newObject("test::TestB"));
    CHECK(obj != nullptr);

    auto objPtr = DrClassMap::getSingleInstance("test::TestB");
    CHECK(objPtr.get() != nullptr);

    auto objPtr2 = DrClassMap::getSingleInstance<::test::TestB>();
    CHECK(objPtr2.get() != nullptr);
    CHECK(objPtr == objPtr2);
}