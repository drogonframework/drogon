#pragma once
#include <trantor/utils/NonCopyable.h>
#include <drogon/DrObject.h>
#include <drogon/utils/string_view.h>

#include <iostream>
#include <set>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <atomic>
#include <future>
#include <iomanip>
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
extern std::mutex mtxRegister;
extern std::promise<void> allTestRan;
extern std::mutex mtxTestStats;
extern bool testHasPrinted;
extern std::set<Case*> registeredTests;
extern std::atomic<size_t> numAssertions;
extern std::atomic<size_t> numCorrectAssertions;
extern size_t numTestCases;
extern std::atomic<size_t> numFailedTestCases;
extern bool printSuccessfulTests;
inline void registerCase(Case* test)
{
    std::unique_lock<std::mutex> l(mtxRegister);
    registeredTests.insert(test);
}

inline void unregisterCase(Case* test)
{
    std::unique_lock<std::mutex> l(mtxRegister);
    registeredTests.erase(test);

    if (registeredTests.empty())
        allTestRan.set_value();
}

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

inline std::string escapeString(const string_view sv)
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

inline std::string prettifyString(const string_view sv, size_t maxLength = 120)
{
    if (sv.size() <= maxLength)
        return "\"" + escapeString(sv) + "\"";

    const std::string msg = "...\" (truncated)";
    return "\"" + escapeString(sv.substr(0, maxLength)) + msg;
}

namespace internal
{
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

}  // namespace internal

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
inline std::string attemptPrint(const std::nullptr_t& v)
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
    template <typename _ = void>  // HACK: prevent this function to be evaulated
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

class ThreadSafeStream final
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

inline ThreadSafeStream print()
{
    return ThreadSafeStream(std::cout);
}

inline ThreadSafeStream printErr()
{
    return ThreadSafeStream(std::cerr);
}

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

void printTestStats();

#ifdef DROGON_TEST_MAIN

namespace internal
{
static std::string leftpad(const std::string& str, size_t len)
{
    if (len <= str.size())
        return str;
    return std::string(len - str.size(), ' ') + str;
}
}  // namespace internal

static void printHelp(string_view argv0)
{
    print() << "A Drogon Test application:\n\n"
            << "Usage: " << argv0 << " [options]\n"
            << "options:\n"
            << "    -r        Run a specific test\n"
            << "    -s        Print successful tests\n"
            << "    -l        List avaliable tests\n"
            << "    -h        Print this help message\n";
}

void printTestStats()
{
    std::unique_lock<std::mutex> lk(internal::mtxTestStats);
    if (internal::testHasPrinted)
        return;
    const size_t successAssertions = internal::numCorrectAssertions;
    const size_t totalAssertions = internal::numAssertions;
    const size_t successTests =
        internal::numTestCases - internal::numFailedTestCases;
    const size_t totalTests = internal::numTestCases;

    float ratio;
    if (totalAssertions != 0)
        ratio = (float)successTests / totalTests;
    else
        ratio = 1;
    const size_t barSize = 80;
    size_t greenBar = barSize * ratio;
    size_t redBar = barSize * (1 - ratio);
    if (greenBar + redBar != barSize)
    {
        float fraction = (ratio * barSize) - (size_t)(ratio * barSize);
        if (fraction >= 0.5f)
            greenBar++;
        else
            redBar++;
    }
    if (successAssertions != totalAssertions && redBar == 0)
    {
        redBar = 1;
        greenBar--;
    }

    print() << "\n\x1B[0;31m" << std::string(redBar, '=') << "\x1B[0;32m"
            << std::string(greenBar, '=') << "\x1B[0m\n";

    if (successAssertions == totalAssertions)
    {
        print() << "\x1B[1;32m  All tests passed\x1B[0m (" << totalAssertions
                << " assertions in " << totalTests << " tests cases).\n";
    }
    else
    {
        std::string totalAssertsionStr = std::to_string(totalAssertions);
        std::string successAssertionsStr = std::to_string(successAssertions);
        std::string failedAssertsionStr =
            std::to_string(totalAssertions - successAssertions);
        std::string totalTestsStr = std::to_string(totalTests);
        std::string successTestsStr = std::to_string(successTests);
        std::string failedTestsStr = std::to_string(totalTests - successTests);
        const size_t totalLen =
            (std::max)(totalAssertsionStr.size(), totalTestsStr.size());
        const size_t successLen =
            (std::max)(successAssertionsStr.size(), successTestsStr.size());
        const size_t failedLen =
            (std::max)(failedAssertsionStr.size(), failedTestsStr.size());
        using internal::leftpad;
        print() << "assertions: " << leftpad(totalAssertsionStr, totalLen)
                << " | \x1B[0;32m" << leftpad(successAssertionsStr, successLen)
                << " passed\x1B[0m | \x1B[0;31m"
                << leftpad(failedAssertsionStr, failedLen) << " failed\x1B[0m\n"
                << "test cases: " << leftpad(totalTestsStr, totalLen)
                << " | \x1B[0;32m" << leftpad(successTestsStr, successLen)
                << " passed\x1B[0m | \x1B[0;31m"
                << leftpad(failedTestsStr, failedLen) << " failed\x1B[0m\n";
    }
    internal::testHasPrinted = true;
}

static int run(int argc, char** argv)
{
    internal::numCorrectAssertions = 0;
    internal::numAssertions = 0;
    internal::numFailedTestCases = 0;
    internal::numTestCases = 0;
    internal::printSuccessfulTests = false;

    std::string targetTest;
    bool listTests = false;
    for (int i = 1; i < argc; i++)
    {
        std::string param = argv[i];
        if (param == "-r" && i + 1 < argc)
        {
            targetTest = argv[i + 1];
            i++;
        }
        if (param == "-h")
        {
            printHelp(argv[0]);
            exit(0);
        }
        if (param == "-s")
        {
            internal::printSuccessfulTests = true;
        }
        if (param == "-l")
        {
            listTests = true;
        }
    }
    auto classNames = DrClassMap::getAllClassName();

    if (listTests)
    {
        print() << "Avaliable Tests:\n";
        for (const auto& name : classNames)
        {
            if (name.find(DROGON_TESTCASE_PREIX_STR_) == 0)
            {
                auto test =
                    std::unique_ptr<DrObjectBase>(DrClassMap::newObject(name));
                auto ptr = dynamic_cast<TestCase*>(test.get());
                if (ptr == nullptr)
                    continue;
                print() << "  " << ptr->name() << "\n";
            }
        }
        exit(0);
    }

    std::vector<std::shared_ptr<TestCase>> testCases;
    // NOTE: Registering a dummy case prevents the test-end signal to be
    // emited too early as there's always an case that hasn't finish
    std::shared_ptr<Case> dummyCase = std::make_shared<Case>("__dummy_dummy_");
    for (const auto& name : classNames)
    {
        if (name.find(DROGON_TESTCASE_PREIX_STR_) == 0)
        {
            auto obj =
                std::shared_ptr<DrObjectBase>(DrClassMap::newObject(name));
            auto test = std::dynamic_pointer_cast<TestCase>(obj);
            if (test == nullptr)
            {
                LOG_WARN << "Class " << name
                         << " seems to be a test case. But type information "
                            "disagrees.";
                continue;
            }
            if (targetTest.empty() || test->name() == targetTest)
            {
                internal::numTestCases++;
                test->doTest_(std::make_shared<Case>(test->name()));
                testCases.emplace_back(std::move(test));
            }
        }
    }
    dummyCase = {};

    if (targetTest != "" && internal::numTestCases == 0)
    {
        printErr() << "Cannot find test named " << targetTest << "\n";
        exit(1);
    }

    if (internal::registeredTests.empty() == false)
    {
        auto fut = internal::allTestRan.get_future();
        fut.get();
        assert(internal::registeredTests.empty());
    }
    testCases.clear();

    printTestStats();

    return internal::numCorrectAssertions != internal::numAssertions;
}
#endif
}  // namespace test
}  // namespace drogon

#define ERROR_MSG(func_name, expr)                                    \
    drogon::test::printErr()                                          \
        << "\x1B[1;37mIn test case " << TEST_CTX->fullname() << "\n"  \
        << "\x1B[0;37m↳ " << __FILE__ << ":" << __LINE__              \
        << " \x1B[0;31m FAILED:\x1B[0m\n"                             \
        << "  \033[0;34m"                                             \
        << drogon::test::internal::stringifyFuncCall(func_name, expr) \
        << "\x1B[0m\n"

#define PASSED_MSG(func_name, expr)                                   \
    drogon::test::print()                                             \
        << "\x1B[1;37mIn test case " << TEST_CTX->fullname() << "\n"  \
        << "\x1B[0;37m↳ " << __FILE__ << ":" << __LINE__              \
        << " \x1B[0;32m PASSED:\x1B[0m\n"                             \
        << "  \033[0;34m"                                             \
        << drogon::test::internal::stringifyFuncCall(func_name, expr) \
        << "\x1B[0m\n"

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
        catch (const std::exception& e)                     \
        {                                                   \
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

#define EVAL_AND_CHECK_TRUE__(func_name, expr)                       \
    do                                                               \
    {                                                                \
        bool drresult__;                                             \
        std::string drexpansion__;                                   \
        DROGON_TEST_START_SUPRESSION_                                \
        DROGON_TEST_SUPPRESS_PARENTHESES_WARNING_                    \
        std::tie(drresult__, drexpansion__) =                        \
            (drogon::test::internal::Decomposer() <= expr).result(); \
        DROGON_TEST_END_SUPRESSION_                                  \
        if (!drresult__)                                             \
        {                                                            \
            ERROR_MSG(func_name, #expr)                              \
                << "With expansion\n"                                \
                << "  \033[0;33m" << drexpansion__ << "\x1B[0m\n\n"; \
        }                                                            \
        else                                                         \
            SET_TEST_SUCCESS__;                                      \
    } while (0);

#define PRINT_UNEXPECTED_EXCEPTION__(func_name, expr)         \
    do                                                        \
    {                                                         \
        ERROR_MSG(func_name, expr)                            \
            << "An unexpected exception is thrown. what():\n" \
            << "  \033[0;33m" << e.what() << "\x1B[0m\n\n";   \
    } while (0);

#define PRINT_PASSED__(func_name, expr)                                 \
    do                                                                  \
    {                                                                   \
        if (drogon::test::internal::printSuccessfulTests && TEST_FLAG_) \
        {                                                               \
            PASSED_MSG(func_name, expr) << "\n";                        \
        }                                                               \
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

#define PRINT_NONSTANDARD_EXCEPTION__(func_name, expr)        \
    do                                                        \
    {                                                         \
        ERROR_MSG(func_name, expr)                            \
            << "Unexpected unknown exception is thrown.\n\n"; \
    } while (0);

#define EVAL__(expr) \
    do               \
    {                \
        expr;        \
    } while (0);

#define NOTHING__ \
    {             \
    }

#define PRINT_ERR_NOEXCEPTION__(expr, func_name)                    \
    do                                                              \
    {                                                               \
        if (!TEST_FLAG_)                                            \
            ERROR_MSG(func_name, expr)                              \
                << "With expecitation\n"                            \
                << "  Expected to throw an exception. But non are " \
                   "thrown.\n\n";                                   \
    } while (0);

#define PRINT_ERR_WITHEXCEPTION__(expr, func_name)                   \
    do                                                               \
    {                                                                \
        if (!TEST_FLAG_)                                             \
            ERROR_MSG(func_name, expr)                               \
                << "With expecitation\n"                             \
                << "  Should to not throw an exception. But one is " \
                   "thrown.\n\n";                                    \
    } while (0);

#define PRINT_ERR_BAD_EXCEPTION__(                                             \
    expr, func_name, excep_type, exceptionThrown, correctExceptionType)        \
    do                                                                         \
    {                                                                          \
        assert((exceptionThrown && correctExceptionType) || !exceptionThrown); \
        if (exceptionThrown == true && correctExceptionType == false)          \
        {                                                                      \
            ERROR_MSG(func_name, expr)                                         \
                << "With expecitation\n"                                       \
                << "  Exception have been throw but not of type \033[0;33m"    \
                << #excep_type << "\033[0m.\n\n";                              \
        }                                                                      \
        else if (exceptionThrown == false)                                     \
        {                                                                      \
            ERROR_MSG(func_name, expr)                                         \
                << "With expecitation\n"                                       \
                << "  A \033[0;33m" << #excep_type                             \
                << "\033[0m exception is expected. But nothing was thrown"     \
                << "\033[0m.\n\n";                                             \
        }                                                                      \
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
                if (dynamic_cast<const except_type*>(&e) != nullptr)          \
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
    } while (0);

#define FAIL(message)                                                   \
    do                                                                  \
    {                                                                   \
        using namespace drogon::test;                                   \
        TEST_CTX->setFailed();                                          \
        printErr() << "\x1B[1;37mIn test case " << TEST_CTX->fullname() \
                   << "\n"                                              \
                   << "\x1B[0;37m" << __FILE__ << ":" << __LINE__       \
                   << " \x1B[0;31m FAILED:\x1B[0m\n"                    \
                   << "  Reason: " << message << "\n\n";                \
        drogon::test::internal::numAssertions++;                        \
    } while (0)
#define FAULT(message)                                             \
    do                                                             \
    {                                                              \
        using namespace drogon::test;                              \
        FAIL(message);                                             \
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

#ifdef DROGON_TEST_MAIN
namespace drogon
{
namespace test
{
std::mutex ThreadSafeStream::mtx_;

namespace internal
{
std::mutex mtxRegister;
std::mutex mtxTestStats;
bool testHasPrinted = false;
std::set<Case*> registeredTests;
std::promise<void> allTestRan;
std::atomic<size_t> numAssertions;
std::atomic<size_t> numCorrectAssertions;
size_t numTestCases;
std::atomic<size_t> numFailedTestCases;
bool printSuccessfulTests;
}  // namespace internal
}  // namespace test
}  // namespace drogon

#endif
