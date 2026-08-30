#include <drogon/DrObject.h>
#include <drogon/drogon_test.h>
#include <drogon/HttpController.h>

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

class TestC : public DrObject<TestC>
{
  public:
    static constexpr bool isAutoCreation = true;
};

class TestD : public DrObject<TestD>
{
  public:
    static constexpr bool isAutoCreation = false;
};

class TestE : public DrObject<TestE>
{
  public:
    static constexpr double isAutoCreation = 3.0;
};

class CtrlA : public HttpController<CtrlA>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_LIST_END
};

class CtrlB : public HttpController<CtrlB, false>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_LIST_END
};

DROGON_TEST(IsAutoCreationClassTest)
{
    STATIC_REQUIRE(isAutoCreationClass<TestA>::value == false);
    STATIC_REQUIRE(isAutoCreationClass<TestC>::value == true);
    STATIC_REQUIRE(isAutoCreationClass<TestD>::value == false);
    STATIC_REQUIRE(isAutoCreationClass<TestE>::value == false);
    STATIC_REQUIRE(isAutoCreationClass<CtrlA>::value == true);
    STATIC_REQUIRE(isAutoCreationClass<CtrlB>::value == false);
}
