#pragma once
#include <trantor/utils/NonCopyable.h>
#include <drogon/DrObject.h>
#include <drogon/exports.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <atomic>
#include <string_view>
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
#if defined(_MSC_VER) && !defined(__clang__)
#define DROGON_TEST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define DROGON_TEST_NOINLINE __attribute__((noinline))
#else
#define DROGON_TEST_NOINLINE
#endif

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

DROGON_EXPORT void registerCase(Case *test);
DROGON_EXPORT void unregisterCase(Case *test);

template <typename _Tp, typename dummy = void>
struct is_printable : std::false_type
{
};

template <typename _Tp>
struct is_printable<
    _Tp,
    std::enable_if_t<std::is_same_v<decltype(std::cout << std::declval<_Tp>()),
                                    std::ostream &>>> : std::true_type
{
};

inline std::string escapeString(const std::string_view sv)
{
    std::string result;
    result.reserve(sv.size());
    for (auto ch : sv)
    {
        if (ch == '\n')
            result += "\\n";
        else if (ch == '\r')
            result += "\\r";
        else if (ch == '\t')
            result += "\\t";
        else if (ch == '\b')
            result += "\\b";
        else if (ch == '\\')
            result += "\\\\";
        else if (ch == '"')
            result += "\"";
        else if (ch == '\v')
            result += "\\v";
        else if (ch == '\a')
            result += "\\a";
        else
            result.push_back(ch);
    }
    return result;
}

DROGON_EXPORT std::string prettifyString(const std::string_view sv,
                                         size_t maxLength = 120);

template <typename... Args>
inline void outputReason(Args &&...args)
{
    (std::cout << ... << std::forward<Args>(args));
}

template <typename T>
inline std::string attemptPrint(T &&v)
{
    using Type = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<Type, std::nullptr_t>)
        return "nullptr";
    else if constexpr (std::is_same_v<Type, char>)
        return "'" + std::string(1, v) + "'";
    else if constexpr (std::is_convertible_v<Type, std::string_view>)
        return prettifyString(v);
    else if constexpr (internal::is_printable<Type>::value)
    {
        std::stringstream ss;
        ss << v;
        return ss.str();
    }
    return "{un-printable}";
}

inline std::string stringifyFuncCall(const std::string &funcName)
{
    return funcName + "()";
}

inline std::string stringifyFuncCall(const std::string &funcName,
                                     const std::string &param1)
{
    return funcName + "(" + param1 + ")";
}

inline std::string stringifyFuncCall(const std::string &funcName,
                                     const std::string &param1,
                                     const std::string &param2)
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

    Lhs(const T &lhs) : ref_(lhs)
    {
    }

    const T &ref_;

// Expression decomposition, one out-of-line instantiation per operand type
// pair. Two properties matter for compile time and are easy to lose:
//   * DROGON_TEST_NOINLINE - without it the whole stringification machinery
//     is inlined into every assertion site.
//   * the expansion string is only built when it will actually be shown.
//     Building it unconditionally costs a formatting pass on every passing
//     assertion, at run time as well as compile time.
#define DROGON_TEST_DECOMPOSE_OP(op)                                      \
    template <typename RhsType>                                           \
    DROGON_TEST_NOINLINE ComparsionResult operator op(const RhsType &rhs) \
    {                                                                     \
        const bool result = ref_ op rhs;                                  \
        if (result && !printSuccessfulTests)                              \
            return ComparsionResult{result, std::string()};               \
        return ComparsionResult{result,                                   \
                                attemptPrint(ref_) + " " #op " " +        \
                                    attemptPrint(rhs)};                   \
    }

    DROGON_TEST_DECOMPOSE_OP(<)
    DROGON_TEST_DECOMPOSE_OP(>)
    DROGON_TEST_DECOMPOSE_OP(<=)
    DROGON_TEST_DECOMPOSE_OP(>=)
    DROGON_TEST_DECOMPOSE_OP(==)
    DROGON_TEST_DECOMPOSE_OP(!=)

#undef DROGON_TEST_DECOMPOSE_OP

    template <typename RhsType>
    ComparsionResult operator&&(const RhsType &rhs)
    {
        static_assert(!std::is_same_v<RhsType, void>,
                      " && is not supported in expression decomposition");
        return {};
    }

    template <typename RhsType>
    ComparsionResult operator||(const RhsType &rhs)
    {
        static_assert(!std::is_same_v<RhsType, void>,
                      " || is not supported in expression decomposition");
        return {};
    }

    template <typename RhsType>
    ComparsionResult operator|(const RhsType &rhs)
    {
        static_assert(!std::is_same_v<RhsType, void>,
                      " | is not supported in expression decomposition");
        return {};
    }

    template <typename RhsType>
    ComparsionResult operator&(const RhsType &rhs)
    {
        static_assert(!std::is_same_v<RhsType, void>,
                      " & is not supported in expression decomposition");
        return {};
    }
};

struct Decomposer
{
    template <typename T>
    Lhs<T> operator<=(const T &other)
    {
        return Lhs<T>(other);
    }
};

}  // namespace internal

class DROGON_EXPORT ThreadSafeStream final
{
  public:
    ThreadSafeStream(std::ostream &os) : os_(os)
    {
        mtx_.lock();
    }

    ~ThreadSafeStream()
    {
        mtx_.unlock();
    }

    template <typename T>
    std::ostream &operator<<(const T &rhs)
    {
        return os_ << rhs;
    }

    static std::mutex mtx_;
    std::ostream &os_;
};

DROGON_EXPORT ThreadSafeStream print();
DROGON_EXPORT ThreadSafeStream printErr();

class CaseBase : public trantor::NonCopyable
{
  public:
    CaseBase() = default;

    CaseBase(const std::string &name) : name_(name)
    {
    }

    CaseBase(std::shared_ptr<CaseBase> parent, const std::string &name)
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

    const std::string &name() const
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

/// Out-of-line assertion reporting.
///
/// Each assertion macro would otherwise inline a cold `operator<<` message
/// chain at every call site - up to four of them per assertion, counting the
/// two exception handlers and the success message. In assertion-heavy test
/// files that dominates compile time and object size.
///
/// Each of these acquires the output lock internally and holds it for exactly
/// one statement, so no locked stream is ever handed across an API boundary.
/// `funcName`/`expr` are `const char *` on purpose: taking `const std::string
/// &` would construct two temporaries at every call site.
DROGON_EXPORT void reportCheckFailure(const CaseBase &testCase,
                                      const char *file,
                                      int line,
                                      const char *funcName,
                                      const char *expr,
                                      const std::string &expansion);
DROGON_EXPORT void reportUnexpectedException(const CaseBase &testCase,
                                             const char *file,
                                             int line,
                                             const char *funcName,
                                             const char *expr,
                                             const char *what);
DROGON_EXPORT void reportUnknownException(const CaseBase &testCase,
                                          const char *file,
                                          int line,
                                          const char *funcName,
                                          const char *expr);
DROGON_EXPORT void reportNoException(const CaseBase &testCase,
                                     const char *file,
                                     int line,
                                     const char *funcName,
                                     const char *expr);
DROGON_EXPORT void reportUnwantedException(const CaseBase &testCase,
                                           const char *file,
                                           int line,
                                           const char *funcName,
                                           const char *expr);
DROGON_EXPORT void reportBadExceptionType(const CaseBase &testCase,
                                          const char *file,
                                          int line,
                                          const char *funcName,
                                          const char *expr,
                                          bool exceptionThrown,
                                          const char *expectedType);

/// Evaluates a decomposed comparison and reports a failure, out of line.
/// Keeping the std::pair<bool, std::string> inside this function rather than at
/// the call site is what stops every assertion from materialising a string (and
/// its destructor and unwind paths) in the caller.
template <typename CmpResult>
DROGON_TEST_NOINLINE bool evalAndReport(const CaseBase &testCase,
                                        const char *file,
                                        int line,
                                        const char *funcName,
                                        const char *expr,
                                        CmpResult &&cmp)
{
    const auto outcome = cmp.result();
    if (!outcome.first)
    {
        reportCheckFailure(
            testCase, file, line, funcName, expr, outcome.second);
        return false;
    }
    return true;
}

DROGON_EXPORT void reportPassed(const CaseBase &testCase,
                                const char *file,
                                int line,
                                const char *funcName,
                                const char *expr);

class Case : public CaseBase
{
  public:
    Case(const std::string &name) : CaseBase(name)
    {
        internal::registerCase(this);
    }

    Case(std::shared_ptr<Case> parent, const std::string &name)
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
    TestCase(const std::string &name) : CaseBase(name)
    {
    }

    virtual ~TestCase() = default;
    virtual void doTest_(std::shared_ptr<Case>) = 0;
};

DROGON_EXPORT void printTestStats();
DROGON_EXPORT int run(int argc, char **argv);
}  // namespace test
}  // namespace drogon

#define SET_TEST_SUCCESS__ \
    do                     \
    {                      \
        TEST_FLAG_ = true; \
    } while (0);

#define TEST_INTERNAL__(func_name,                          \
                        expr,                               \
                        eval,                               \
                        on_exception,                       \
                        on_non_standard_exception,          \
                        on_leaving)                         \
    do                                                      \
    {                                                       \
        bool TEST_FLAG_ = false;                            \
        using drogon::test::internal::stringifyFuncCall;    \
        using drogon::test::printErr;                       \
        drogon::test::internal::numAssertions++;            \
        try                                                 \
        {                                                   \
            eval;                                           \
        }                                                   \
        catch (const std::exception &e)                     \
        {                                                   \
            (void)e;                                        \
            on_exception;                                   \
        }                                                   \
        catch (...)                                         \
        {                                                   \
            on_non_standard_exception;                      \
        }                                                   \
        if (TEST_FLAG_)                                     \
            drogon::test::internal::numCorrectAssertions++; \
        else                                                \
            TEST_CTX->setFailed();                          \
        on_leaving;                                         \
    } while (0);

#define EVAL_AND_CHECK_TRUE__(func_name, expr)                   \
    do                                                           \
    {                                                            \
        DROGON_TEST_START_SUPRESSION_                            \
        DROGON_TEST_SUPPRESS_PARENTHESES_WARNING_                \
        if (drogon::test::evalAndReport(                         \
                *TEST_CTX,                                       \
                __FILE__,                                        \
                __LINE__,                                        \
                func_name,                                       \
                #expr,                                           \
                (drogon::test::internal::Decomposer() <= expr))) \
            SET_TEST_SUCCESS__;                                  \
        DROGON_TEST_END_SUPRESSION_                              \
    } while (0);

#define PRINT_UNEXPECTED_EXCEPTION__(func_name, expr)                  \
    do                                                                 \
    {                                                                  \
        drogon::test::reportUnexpectedException(                       \
            *TEST_CTX, __FILE__, __LINE__, func_name, expr, e.what()); \
    } while (0);

#define PRINT_PASSED__(func_name, expr)                                 \
    do                                                                  \
    {                                                                   \
        if (drogon::test::internal::printSuccessfulTests && TEST_FLAG_) \
            drogon::test::reportPassed(                                 \
                *TEST_CTX, __FILE__, __LINE__, func_name, expr);        \
    } while (0);

#define RETURN_ON_FAILURE__ \
    do                      \
    {                       \
        if (!TEST_FLAG_)    \
            return;         \
    } while (0);

#define CO_RETURN_ON_FAILURE__ \
    do                         \
    {                          \
        if (!TEST_FLAG_)       \
            co_return;         \
    } while (0);

#define DIE_ON_FAILURE__                                                \
    do                                                                  \
    {                                                                   \
        using namespace drogon::test;                                   \
        if (!TEST_FLAG_)                                                \
        {                                                               \
            printTestStats();                                           \
            printErr() << "Force exiting due to a mandation failed.\n"; \
            exit(1);                                                    \
        }                                                               \
    } while (0);

#define PRINT_NONSTANDARD_EXCEPTION__(func_name, expr)       \
    do                                                       \
    {                                                        \
        drogon::test::reportUnknownException(                \
            *TEST_CTX, __FILE__, __LINE__, func_name, expr); \
    } while (0);

#define EVAL__(expr) \
    do               \
    {                \
        expr;        \
    } while (0);

#define NOTHING__ \
    {             \
    }

#define PRINT_ERR_NOEXCEPTION__(expr, func_name)                 \
    do                                                           \
    {                                                            \
        if (!TEST_FLAG_)                                         \
            drogon::test::reportNoException(                     \
                *TEST_CTX, __FILE__, __LINE__, func_name, expr); \
    } while (0);

#define PRINT_ERR_WITHEXCEPTION__(expr, func_name)               \
    do                                                           \
    {                                                            \
        if (!TEST_FLAG_)                                         \
            drogon::test::reportUnwantedException(               \
                *TEST_CTX, __FILE__, __LINE__, func_name, expr); \
    } while (0);

#define PRINT_ERR_BAD_EXCEPTION__(                                             \
    expr, func_name, excep_type, exceptionThrown, correctExceptionType)        \
    do                                                                         \
    {                                                                          \
        assert((exceptionThrown && correctExceptionType) || !exceptionThrown); \
        if (!(exceptionThrown) || !(correctExceptionType))                     \
            drogon::test::reportBadExceptionType(*TEST_CTX,                    \
                                                 __FILE__,                     \
                                                 __LINE__,                     \
                                                 func_name,                    \
                                                 expr,                         \
                                                 (exceptionThrown),            \
                                                 #excep_type);                 \
    } while (0);

#define CHECK_INTERNAL__(expr, func_name, on_leave)                      \
    do                                                                   \
    {                                                                    \
        TEST_INTERNAL__(func_name,                                       \
                        expr,                                            \
                        EVAL_AND_CHECK_TRUE__(func_name, expr),          \
                        PRINT_UNEXPECTED_EXCEPTION__(func_name, #expr),  \
                        PRINT_NONSTANDARD_EXCEPTION__(func_name, #expr), \
                        on_leave PRINT_PASSED__(func_name, #expr));      \
    } while (0)

#define CHECK_THROWS_INTERNAL__(expr, func_name, on_leave)              \
    do                                                                  \
    {                                                                   \
        TEST_INTERNAL__(func_name,                                      \
                        expr,                                           \
                        EVAL__(expr),                                   \
                        SET_TEST_SUCCESS__,                             \
                        SET_TEST_SUCCESS__,                             \
                        PRINT_ERR_NOEXCEPTION__(#expr, func_name)       \
                            on_leave PRINT_PASSED__(func_name, #expr)); \
    } while (0)

#define CHECK_THROWS_AS_INTERNAL__(expr, func_name, except_type, on_leave)    \
    do                                                                        \
    {                                                                         \
        bool exceptionThrown = false;                                         \
        TEST_INTERNAL__(                                                      \
            func_name,                                                        \
            expr,                                                             \
            EVAL__(expr),                                                     \
            {                                                                 \
                exceptionThrown = true;                                       \
                if (dynamic_cast<const except_type *>(&e) != nullptr)         \
                    SET_TEST_SUCCESS__;                                       \
            },                                                                \
            { exceptionThrown = true; },                                      \
            PRINT_ERR_BAD_EXCEPTION__(#expr ", " #except_type,                \
                                      func_name,                              \
                                      except_type,                            \
                                      exceptionThrown,                        \
                                      TEST_FLAG_)                             \
                on_leave PRINT_PASSED__(func_name, #expr ", " #except_type)); \
    } while (0)

#define CHECK_NOTHROW_INTERNAL__(expr, func_name, on_leave)             \
    do                                                                  \
    {                                                                   \
        TEST_INTERNAL__(func_name,                                      \
                        expr,                                           \
                        EVAL__(expr) SET_TEST_SUCCESS__,                \
                        NOTHING__,                                      \
                        NOTHING__,                                      \
                        PRINT_ERR_WITHEXCEPTION__(#expr, func_name)     \
                            on_leave PRINT_PASSED__(func_name, #expr)); \
    } while (0)

#define CHECK(expr) CHECK_INTERNAL__(expr, "CHECK", NOTHING__)
#define CHECK_THROWS(expr) \
    CHECK_THROWS_INTERNAL__(expr, "CHECK_THROWS", NOTHING__)
#define CHECK_NOTHROW(expr) \
    CHECK_NOTHROW_INTERNAL__(expr, "CHECK_NOTHROW", NOTHING__)
#define CHECK_THROWS_AS(expr, except_type) \
    CHECK_THROWS_AS_INTERNAL__(expr, "CHECK_THROWS_AS", except_type, NOTHING__)

#define REQUIRE(expr) CHECK_INTERNAL__(expr, "REQUIRE", RETURN_ON_FAILURE__)
#define REQUIRE_THROWS(expr) \
    CHECK_THROWS_INTERNAL__(expr, "REQUIRE_THROWS", RETURN_ON_FAILURE__)
#define REQUIRE_NOTHROW(expr) \
    CHECK_NOTHROW_INTERNAL__(expr, "REQUIRE_NOTHROW", RETURN_ON_FAILURE__)
#define REQUIRE_THROWS_AS(expr, except_type)        \
    CHECK_THROWS_AS_INTERNAL__(expr,                \
                               "REQUIRE_THROWS_AS", \
                               except_type,         \
                               RETURN_ON_FAILURE__)

#define CO_REQUIRE(expr) \
    CHECK_INTERNAL__(expr, "CO_REQUIRE", CO_RETURN_ON_FAILURE__)
#define CO_REQUIRE_THROWS(expr) \
    CHECK_THROWS_INTERNAL__(expr, "CO_REQUIRE_THROWS", CO_RETURN_ON_FAILURE__)
#define CO_REQUIRE_NOTHROW(expr) \
    CHECK_NOTHROW_INTERNAL__(expr, "CO_REQUIRE_NOTHROW", CO_RETURN_ON_FAILURE__)
#define CO_REQUIRE_THROWS_AS(expr, except_type)        \
    CHECK_THROWS_AS_INTERNAL__(expr,                   \
                               "CO_REQUIRE_THROWS_AS", \
                               except_type,            \
                               CO_RETURN_ON_FAILURE__)

#define MANDATE(expr) CHECK_INTERNAL__(expr, "MANDATE", DIE_ON_FAILURE__)
#define MANDATE_THROWS(expr) \
    CHECK_THROWS_INTERNAL__(expr, "MANDATE_THROWS", DIE_ON_FAILURE__)
#define MANDATE_NOTHROW(expr) \
    CHECK_NOTHROW_INTERNAL__(expr, "MANDATE_NOTHROW", DIE_ON_FAILURE__)
#define MANDATE_THROWS_AS(expr, except_type)        \
    CHECK_THROWS_AS_INTERNAL__(expr,                \
                               "MANDATE_THROWS_AS", \
                               except_type,         \
                               DIE_ON_FAILURE__)

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
        using namespace drogon::test;                                   \
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
        using namespace drogon::test;                              \
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
