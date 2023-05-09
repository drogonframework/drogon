#pragma once
#include <trantor/utils/NonCopyable.h>
#include <drogon/DrObject.h>
#include <drogon/utils/string_view.h>
#include <drogon/exports.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <atomic>
#include <cstddef>
/**
 * @brief Drogon Test is a minimal effort test framework developed because the
 * major C++ test frameworks doesn't handle async programs well. Drogon Test's
 * syntax is inspired by both Google Test and Catch2
 */
namespace drogon
{
namespace test
{
#define TEST_CTX drogon_test_ctx_
#define DROGON_TESTCASE_PREIX_ drtest__
#define DROGON_TESTCASE_PREIX_STR_ "drtest__"
#define TEST_FLAG_ drgood__

#define DROGON_TEST_STRINGIFY__(x) #x
#define DROGON_TEST_STRINGIFY(x) DROGON_TEST_STRINGIFY__(x)
#define DROGON_TEST_CONCAT__(a, b) a##b
#define DROGON_TEST_CONCAT(a, b) DROGON_TEST_CONCAT__(a, b)

#if defined(__GNUC__) && !defined(__clang__) && !defined(__ICC)
#define DROGON_TEST_START_SUPRESSION_ _Pragma("GCC diagnostic push")
#define DROGON_TEST_END_SUPRESSION_ _Pragma("GCC diagnostic pop")
#define DROGON_TEST_SUPPRESS_PARENTHESES_WARNING_ \
    _Pragma("GCC diagnostic ignored \"-Wparentheses\"")
#define DROGON_TEST_SUPPRESS_UNUSED_VALUE_WARNING_ \
    _Pragma("GCC diagnostic ignored \"-Wunused-value\"")
#elif defined(__clang__) && !defined(_MSC_VER)
#define DROGON_TEST_START_SUPRESSION_ _Pragma("clang diagnostic push")
#define DROGON_TEST_END_SUPRESSION_ _Pragma("clang diagnostic pop")
#define DROGON_TEST_SUPPRESS_PARENTHESES_WARNING_ \
    _Pragma("clang diagnostic ignored \"-Wparentheses\"")
#define DROGON_TEST_SUPPRESS_UNUSED_VALUE_WARNING_ \
    _Pragma("clang diagnostic ignored \"-Wunused-value\"")
// MSVC don't have an equlivent. Add other compilers here
#else
#define DROGON_TEST_START_SUPRESSION_
#define DROGON_TEST_END_SUPRESSION_
#define DROGON_TEST_SUPPRESS_PARENTHESES_WARNING_
#define DROGON_TEST_SUPPRESS_UNUSED_VALUE_WARNING_
#endif

class Case;

namespace internal
{
DROGON_EXPORT extern std::atomic<size_t> numAssertions;
DROGON_EXPORT extern std::atomic<size_t> numCorrectAssertions;
DROGON_EXPORT extern std::atomic<size_t> numFailedTestCases;
DROGON_EXPORT extern bool printSuccessfulTests;

DROGON_EXPORT void registerCase(Case* test);
DROGON_EXPORT void unregisterCase(Case* test);

template <typename _Tp, typename dummy = void>
struct is_printable : std::false_type
{
};

template <typename _Tp>
struct is_printable<_Tp,
                    typename std::enable_if<
                        std::is_same<decltype(std::cout << std::declval<_Tp>()),
                                     std::ostream&>::value>::type>
    : std::true_type
{
};

DROGON_EXPORT std::string prettifyString(const string_view sv,
                                         size_t maxLength = 120);

#ifdef __cpp_fold_expressions
template <typename... Args>
inline void outputReason(Args&&... args)
{
    (std::cout << ... << std::forward<Args>(args));
}
#else
template <typename Head>
inline void outputReason(Head&& head)
{
    std::cout << std::forward<Head>(head);
}
template <typename Head, typename... Tail>
inline void outputReason(Head&& head, Tail&&... tail)
{
    std::cout << std::forward<Head>(head);
    outputReason(std::forward<Tail>(tail)...);
}
#endif

template <bool P>
struct AttemptPrintViaStream
{
    template <typename T>
    std::string operator()(const T& v)
    {
        return "{un-printable}";
    }
};

template <>
struct AttemptPrintViaStream<true>
{
    template <typename T>
    std::string operator()(const T& v)
    {
        std::stringstream ss;
        ss << v;
        return ss.str();
    }
};

struct StringPrinter
{
    std::string operator()(const string_view& v)
    {
        return prettifyString(v);
    }
};

template <typename T>
inline std::string attemptPrint(T&& v)
{
    using DefaultPrinter =
        internal::AttemptPrintViaStream<is_printable<T>::value>;

    // Poor man's if constexpr because SFINAE don't disambiguate between
    // possible resolutions
    return typename std::conditional<std::is_convertible<T, string_view>::value,
                                     internal::StringPrinter,
                                     DefaultPrinter>::type()(v);
}

// Specializations to reduce template construction
template <>
inline std::string attemptPrint(const std::nullptr_t&)
{
    return "nullptr";
}

template <>
inline std::string attemptPrint(const char& v)
{
    return "'" + std::string(1, v) + "'";
}

inline std::string stringifyFuncCall(const std::string& funcName)
{
    return funcName + "()";
}

inline std::string stringifyFuncCall(const std::string& funcName,
                                     const std::string& param1)
{
    return funcName + "(" + param1 + ")";
}

inline std::string stringifyFuncCall(const std::string& funcName,
                                     const std::string& param1,
                                     const std::string& param2)
{
    return funcName + "(" + param1 + ", " + param2 + ")";
}

struct ComparsionResult
{
    std::pair<bool, std::string> result() const
    {
        return {comparsionResilt_, expansion_};
    }

    bool comparsionResilt_;
    std::string expansion_;
};

template <typename T>
struct Lhs
{
    template <typename _ = void>  // HACK: prevent this function to be evaluated
                                  // when not invoked
    std::pair<bool, std::string> result() const
    {
        return {(bool)ref_, attemptPrint(ref_)};
    }

    Lhs(const T& lhs) : ref_(lhs)
    {
    }
    const T& ref_;

    template <typename RhsType>
    ComparsionResult operator<(const RhsType& rhs)
    {
        return ComparsionResult{ref_ < rhs,
                                attemptPrint(ref_) + " < " +
                                    attemptPrint(ref_)};
    }

    template <typename RhsType>
    ComparsionResult operator>(const RhsType& rhs)
    {
        return ComparsionResult{ref_ > rhs,
                                attemptPrint(ref_) + " > " + attemptPrint(rhs)};
    }

    template <typename RhsType>
    ComparsionResult operator<=(const RhsType& rhs)
    {
        return ComparsionResult{ref_ <= rhs,
                                attemptPrint(ref_) +
                                    " <= " + attemptPrint(rhs)};
    }

    template <typename RhsType>
    ComparsionResult operator>=(const RhsType& rhs)
    {
        return ComparsionResult{ref_ >= rhs,
                                attemptPrint(ref_) +
                                    " >= " + attemptPrint(rhs)};
    }

    template <typename RhsType>
    ComparsionResult operator==(const RhsType& rhs)
    {
        return ComparsionResult{ref_ == rhs,
                                attemptPrint(ref_) +
                                    " == " + attemptPrint(rhs)};
    }

    template <typename RhsType>
    ComparsionResult operator!=(const RhsType& rhs)
    {
        return ComparsionResult{ref_ != rhs,
                                attemptPrint(ref_) +
                                    " != " + attemptPrint(rhs)};
    }

    template <typename RhsType>
    ComparsionResult operator&&(const RhsType& rhs)
    {
        static_assert(!std::is_same<RhsType, void>::value,
                      " && is not supported in expression decomposition");
        return {};
    }

    template <typename RhsType>
    ComparsionResult operator||(const RhsType& rhs)
    {
        static_assert(!std::is_same<RhsType, void>::value,
                      " || is not supported in expression decomposition");
        return {};
    }

    template <typename RhsType>
    ComparsionResult operator|(const RhsType& rhs)
    {
        static_assert(!std::is_same<RhsType, void>::value,
                      " | is not supported in expression decomposition");
        return {};
    }

    template <typename RhsType>
    ComparsionResult operator&(const RhsType& rhs)
    {
        static_assert(!std::is_same<RhsType, void>::value,
                      " & is not supported in expression decomposition");
        return {};
    }
};

struct Decomposer
{
    template <typename T>
    Lhs<T> operator<=(const T& other)
    {
        return Lhs<T>(other);
    }
};

}  // namespace internal

class DROGON_EXPORT ThreadSafeStream final
{
  public:
    ThreadSafeStream(std::ostream& os) : os_(os)
    {
        mtx_.lock();
    }

    ~ThreadSafeStream()
    {
        mtx_.unlock();
    }

    template <typename T>
    std::ostream& operator<<(const T& rhs)
    {
        return os_ << rhs;
    }

    static std::mutex mtx_;
    std::ostream& os_;
};

DROGON_EXPORT ThreadSafeStream print();
DROGON_EXPORT ThreadSafeStream printErr();

class CaseBase : public trantor::NonCopyable
{
  public:
    CaseBase() = default;
    CaseBase(const std::string& name) : name_(name)
    {
    }
    CaseBase(std::shared_ptr<CaseBase> parent, const std::string& name)
        : parent_(parent), name_(name)
    {
    }
    virtual ~CaseBase() = default;

    std::string fullname() const
    {
        std::string result;
        auto curr = this;
        while (curr != nullptr)
        {
            result = curr->name() + result;
            if (curr->parent_ != nullptr)
                result = "." + result;
            curr = curr->parent_.get();
        }
        return result;
    }

    const std::string& name() const
    {
        return name_;
    }

    void setFailed()
    {
        if (failed_ == false)
        {
            internal::numFailedTestCases++;
            failed_ = true;
        }
    }

    bool failed() const
    {
        return failed_;
    }

  protected:
    bool failed_ = false;
    std::shared_ptr<CaseBase> parent_ = nullptr;
    std::string name_;
};

class Case : public CaseBase
{
  public:
    Case(const std::string& name) : CaseBase(name)
    {
        internal::registerCase(this);
    }

    Case(std::shared_ptr<Case> parent, const std::string& name)
        : CaseBase(parent, name)
    {
        internal::registerCase(this);
    }

    virtual ~Case()
    {
        internal::unregisterCase(this);
    }
};

struct TestCase : public CaseBase
{
    TestCase(const std::string& name) : CaseBase(name)
    {
    }
    virtual ~TestCase() = default;
    virtual void doTest_(std::shared_ptr<Case>) = 0;
};

DROGON_EXPORT void printTestStats();
DROGON_EXPORT int run(int argc, char** argv);
}  // namespace test
}  // namespace drogon

#define ERROR_MSG(func_name, expr, codeloc)                            \
    drogon::test::printErr()                                           \
        << "\x1B[1;37mIn test case " << TEST_CTX->fullname() << "\n"   \
        << "\x1B[0;37m↳ " << codeloc << " \x1B[0;31m FAILED:\x1B[0m\n" \
        << "  \033[0;34m"                                              \
        << drogon::test::internal::stringifyFuncCall(func_name, expr)  \
        << "\x1B[0m\n"

#define PASSED_MSG(func_name, expr, codeloc)                           \
    drogon::test::print()                                              \
        << "\x1B[1;37mIn test case " << TEST_CTX->fullname() << "\n"   \
        << "\x1B[0;37m↳ " << codeloc << " \x1B[0;32m PASSED:\x1B[0m\n" \
        << "  \033[0;34m"                                              \
        << drogon::test::internal::stringifyFuncCall(func_name, expr)  \
        << "\x1B[0m\n"

#define STATIC_REQUIRE(expr)                            \
    do                                                  \
    {                                                   \
        DROGON_TEST_START_SUPRESSION_                   \
        DROGON_TEST_SUPPRESS_UNUSED_VALUE_WARNING_      \
        TEST_CTX;                                       \
        DROGON_TEST_END_SUPRESSION_                     \
        drogon::test::internal::numAssertions++;        \
        static_assert((expr), #expr " failed.");        \
        drogon::test::internal::numCorrectAssertions++; \
    } while (0)

#define FAIL(...)                                                       \
    do                                                                  \
    {                                                                   \
        using namespace ::drogon::test;                                 \
        TEST_CTX->setFailed();                                          \
        printErr() << "\x1B[1;37mIn test case " << TEST_CTX->fullname() \
                   << "\n"                                              \
                   << "\x1B[0;37m" << __FILE__ << ":" << __LINE__       \
                   << " \x1B[0;31m FAILED:\x1B[0m\n"                    \
                   << "  Reason: ";                                     \
        drogon::test::internal::outputReason(__VA_ARGS__);              \
        printErr() << "\n\n";                                           \
        drogon::test::internal::numAssertions++;                        \
    } while (0)
#define FAULT(...)                                                 \
    do                                                             \
    {                                                              \
        using namespace ::drogon::test;                            \
        FAIL(__VA_ARGS__);                                         \
        printTestStats();                                          \
        printErr() << "Force exiting due to a FAULT statement.\n"; \
        exit(1);                                                   \
    } while (0)

#define SUCCESS()                                                            \
    do                                                                       \
    {                                                                        \
        DROGON_TEST_START_SUPRESSION_                                        \
        DROGON_TEST_SUPPRESS_UNUSED_VALUE_WARNING_                           \
        TEST_CTX;                                                            \
        DROGON_TEST_END_SUPRESSION_                                          \
        if (drogon::test::internal::printSuccessfulTests)                    \
            drogon::test::print()                                            \
                << "\x1B[1;37mIn test case " << TEST_CTX->fullname() << "\n" \
                << "\x1B[0;37m↳ " << __FILE__ << ":" << __LINE__             \
                << " \x1B[0;32m PASSED:\x1B[0m\n"                            \
                << "  \033[0;34mSUCCESS()\x1B[0m\n\n";                       \
        drogon::test::internal::numAssertions++;                             \
        drogon::test::internal::numCorrectAssertions++;                      \
    } while (0)

namespace drogon::test::internal
{
bool DROGON_EXPORT
check_internal(std::string func_name,
               std::string expr,
               const std::shared_ptr<drogon::test::CaseBase>& TEST_CTX,
               std::function<std::pair<bool, std::string>()> functor,
               std::string codeloc);
bool DROGON_EXPORT
throws_internal(std::string func_name,
                std::string expr,
                const std::shared_ptr<drogon::test::CaseBase>& TEST_CTX,
                std::function<void()> functor,
                std::string codeloc);
bool DROGON_EXPORT
nothrow_internal(std::string func_name,
                 std::string expr,
                 const std::shared_ptr<drogon::test::CaseBase>& TEST_CTX,
                 std::function<void()> functor,
                 std::string codeloc);
bool DROGON_EXPORT
throws_as_internal(std::string func_name,
                   std::string expr,
                   const std::shared_ptr<drogon::test::CaseBase>& TEST_CTX,
                   std::function<void()> functor,
                   std::string codeloc,
                   bool (*validator)(std::exception*));
[[noreturn]] void DROGON_EXPORT mandateExit();
}  // namespace drogon::test::internal

#define DRTEST_MAKE_FUNCTOR(expr)                                       \
    [&] {                                                               \
        DROGON_TEST_START_SUPRESSION_                                   \
        DROGON_TEST_SUPPRESS_PARENTHESES_WARNING_                       \
        return (drogon::test::internal::Decomposer() <= expr).result(); \
        DROGON_TEST_END_SUPRESSION_                                     \
    }

#define DRTEST_MAKE_SIMPLE_FUNCTOR(expr)           \
    [&] {                                          \
        DROGON_TEST_START_SUPRESSION_              \
        DROGON_TEST_SUPPRESS_UNUSED_VALUE_WARNING_ \
        expr;                                      \
        DROGON_TEST_END_SUPRESSION_                \
    }

#define PRINT_PASSED__(func_name, expr, codeloc)          \
    do                                                    \
    {                                                     \
        if (drogon::test::internal::printSuccessfulTests) \
        {                                                 \
            PASSED_MSG(func_name, expr, codeloc) << "\n"; \
        }                                                 \
    } while (0);

#define DRCODE_LOCATION__ __FILE__ ":" DROGON_TEST_STRINGIFY(__LINE__)
#define CHECK(expr)                                                     \
    ::drogon::test::internal::check_internal("CHECK",                   \
                                             #expr,                     \
                                             TEST_CTX,                  \
                                             DRTEST_MAKE_FUNCTOR(expr), \
                                             DRCODE_LOCATION__)
#define CHECK_THROWS(expr)                                                \
    ::drogon::test::internal::throws_internal("CHECK_THROWS",             \
                                              #expr,                      \
                                              TEST_CTX,                   \
                                              DRTEST_MAKE_SIMPLE_FUNCTOR( \
                                                  expr),                  \
                                              DRCODE_LOCATION__)
#define CHECK_NOTHROW(expr)                                                \
    ::drogon::test::internal::nothrow_internal("CHECK_NOTHROW",            \
                                               #expr,                      \
                                               TEST_CTX,                   \
                                               DRTEST_MAKE_SIMPLE_FUNCTOR( \
                                                   expr),                  \
                                               DRCODE_LOCATION__)
#define CHECK_THROWS_AS(expr, expect_type)                   \
    ::drogon::test::internal::throws_as_internal(            \
        "CHECK_THROWS",                                      \
        #expr,                                               \
        TEST_CTX,                                            \
        DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                    \
        DRCODE_LOCATION__,                                   \
        +[](std::exception* e) {                             \
            return dynamic_cast<expect_type*>(e) != nullptr; \
        })

#define REQUIRE(expr)                                                     \
    do                                                                    \
    {                                                                     \
        bool ok =                                                         \
            ::drogon::test::internal::check_internal("REQUIRE",           \
                                                     #expr,               \
                                                     TEST_CTX,            \
                                                     DRTEST_MAKE_FUNCTOR( \
                                                         expr),           \
                                                     DRCODE_LOCATION__);  \
        if (!ok)                                                          \
            return;                                                       \
    } while (0)
#define REQUIRE_THROWS(expr)                                 \
    do                                                       \
    {                                                        \
        bool ok = ::drogon::test::internal::throws_internal( \
            "REQUIRE_THROWS",                                \
            #expr,                                           \
            TEST_CTX,                                        \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                \
            DRCODE_LOCATION__);                              \
        if (!ok)                                             \
            return;                                          \
    } while (0)
#define REQUIRE_NOTHROW(expr)                                 \
    do                                                        \
    {                                                         \
        bool ok = ::drogon::test::internal::nothrow_internal( \
            "REQUIRE_NOTHROW",                                \
            #expr,                                            \
            TEST_CTX,                                         \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                 \
            DRCODE_LOCATION__);                               \
        if (!ok)                                              \
            return;                                           \
    } while (0)
#define REQUIRE_THROWS_AS(expr, expect_type)                     \
    do                                                           \
    {                                                            \
        bool ok = ::drogon::test::internal::throws_as_internal(  \
            "REQUIRE_THROWS",                                    \
            #expr,                                               \
            TEST_CTX,                                            \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                    \
            DRCODE_LOCATION__,                                   \
            +[](std::exception* e) {                             \
                return dynamic_cast<expect_type*>(e) != nullptr; \
            });                                                  \
        if (!ok)                                                 \
            return;                                              \
    } while (0)

#define CO_REQUIRE(expr)                                                  \
    do                                                                    \
    {                                                                     \
        bool ok =                                                         \
            ::drogon::test::internal::check_internal("REQUIRE",           \
                                                     #expr,               \
                                                     TEST_CTX,            \
                                                     DRTEST_MAKE_FUNCTOR( \
                                                         expr),           \
                                                     DRCODE_LOCATION__);  \
        if (!ok)                                                          \
            co_return;                                                    \
    } while (0)
#define CO_REQUIRE_THROWS(expr)                              \
    do                                                       \
    {                                                        \
        bool ok = ::drogon::test::internal::throws_internal( \
            "REQUIRE_THROWS",                                \
            #expr,                                           \
            TEST_CTX,                                        \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                \
            DRCODE_LOCATION__);                              \
        if (!ok)                                             \
            co_return;                                       \
    } while (0)
#define CO_REQUIRE_NOTHROW(expr)                              \
    do                                                        \
    {                                                         \
        bool ok = ::drogon::test::internal::nothrow_internal( \
            "REQUIRE_NOTHROW",                                \
            #expr,                                            \
            TEST_CTX,                                         \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                 \
            DRCODE_LOCATION__);                               \
        if (!ok)                                              \
            co_return;                                        \
    } while (0)
#define CO_REQUIRE_THROWS_AS(expr, expect_type)                  \
    do                                                           \
    {                                                            \
        bool ok = ::drogon::test::internal::throws_as_internal(  \
            "REQUIRE_THROWS",                                    \
            #expr,                                               \
            TEST_CTX,                                            \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                    \
            DRCODE_LOCATION__,                                   \
            +[](std::exception* e) {                             \
                return dynamic_cast<expect_type*>(e) != nullptr; \
            });                                                  \
        if (!ok)                                                 \
            co_return;                                           \
    } while (0)

#define MANDATE(expr)                                                     \
    do                                                                    \
    {                                                                     \
        bool ok =                                                         \
            ::drogon::test::internal::check_internal("MANDATE",           \
                                                     #expr,               \
                                                     TEST_CTX,            \
                                                     DRTEST_MAKE_FUNCTOR( \
                                                         expr),           \
                                                     DRCODE_LOCATION__);  \
        if (!ok)                                                          \
            ::drogon::test::internal::mandateExit();                      \
    } while (0)
#define MANDATE_THROWS(expr)                                 \
    do                                                       \
    {                                                        \
        bool ok = ::drogon::test::internal::throws_internal( \
            "MANDATE_THROWS",                                \
            #expr,                                           \
            TEST_CTX,                                        \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                \
            DRCODE_LOCATION__);                              \
        if (!ok)                                             \
            ::drogon::test::internal::mandateExit();         \
    } while (0)
#define MANDATE_NOTHROW(expr)                                 \
    do                                                        \
    {                                                         \
        bool ok = ::drogon::test::internal::nothrow_internal( \
            "MANDATE_NOTHROW",                                \
            #expr,                                            \
            TEST_CTX,                                         \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                 \
            DRCODE_LOCATION__);                               \
        if (!ok)                                              \
            ::drogon::test::internal::mandateExit();          \
    } while (0)
#define MANDATE_THROWS_AS(expr, expect_type)                     \
    do                                                           \
    {                                                            \
        bool ok = ::drogon::test::internal::throws_as_internal(  \
            "MANDATE_THROWS",                                    \
            #expr,                                               \
            TEST_CTX,                                            \
            DRTEST_MAKE_SIMPLE_FUNCTOR(expr),                    \
            DRCODE_LOCATION__,                                   \
            +[](std::exception* e) {                             \
                return dynamic_cast<expect_type*>(e) != nullptr; \
            });                                                  \
        if (!ok)                                                 \
            ::drogon::test::internal::mandateExit();             \
    } while (0)

#define DROGON_TEST_CLASS_NAME_(test_name) \
    DROGON_TEST_CONCAT(DROGON_TESTCASE_PREIX_, test_name)

#define DROGON_TEST(test_name)                                             \
    struct DROGON_TEST_CLASS_NAME_(test_name)                              \
        : public drogon::DrObject<DROGON_TEST_CLASS_NAME_(test_name)>,     \
          public drogon::test::TestCase                                    \
    {                                                                      \
        DROGON_TEST_CLASS_NAME_(test_name)                                 \
        () : drogon::test::TestCase(#test_name)                            \
        {                                                                  \
        }                                                                  \
        inline void doTest_(std::shared_ptr<drogon::test::Case>) override; \
    };                                                                     \
    void DROGON_TEST_CLASS_NAME_(test_name)::doTest_(                      \
        std::shared_ptr<drogon::test::Case> TEST_CTX)
#define SUBTEST(name) (std::make_shared<drogon::test::Case>(TEST_CTX, name))
#define SUBSECTION(name)                                                 \
    for (std::shared_ptr<drogon::test::Case> ctx_hold__ = TEST_CTX,      \
                                             ctx_tmp__ = SUBTEST(#name); \
         ctx_tmp__ != nullptr;                                           \
         TEST_CTX = ctx_hold__, ctx_tmp__ = nullptr)                     \
        if (TEST_CTX = ctx_tmp__, TEST_CTX != nullptr)
