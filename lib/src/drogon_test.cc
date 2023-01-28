#include <drogon/drogon_test.h>

#include <set>
#include <future>
#include <condition_variable>

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

void registerCase(Case* test)
{
    std::unique_lock<std::mutex> l(mtxRegister);
    registeredTests.insert(test);
}

void unregisterCase(Case* test)
{
    std::unique_lock<std::mutex> l(mtxRegister);
    registeredTests.erase(test);

    if (registeredTests.empty())
        allTestRan.set_value();
}

static std::string leftpad(const std::string& str, size_t len)
{
    if (len <= str.size())
        return str;
    return std::string(len - str.size(), ' ') + str;
}

std::string prettifyString(const string_view sv, size_t maxLength)
{
    if (sv.size() <= maxLength)
        return "\"" + escapeString(sv) + "\"";

    const std::string msg = "...\" (truncated)";
    return "\"" + escapeString(sv.substr(0, maxLength)) + msg;
}

}  // namespace internal

static void printHelp(string_view argv0)
{
    print() << "A Drogon Test application:\n\n"
            << "Usage: " << argv0 << " [options]\n"
            << "options:\n"
            << "    -r            Run a specific test\n"
            << "    -s            Print successful tests\n"
            << "    -l            List avaliable tests\n"
            << "    -h | --help   Print this help message\n";
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
    auto greenBar = size_t(barSize * ratio);
    auto redBar = size_t(barSize * (1 - ratio));
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

int run(int argc, char** argv)
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
        const std::string param = argv[i];
        if (param == "-r")
        {
            if (!targetTest.empty())
            {
                printErr() << "Only one test can be specified to run\n";
                exit(1);
            }
            else if (i + 1 >= argc)
            {
                printErr() << "Missing test name after -r.\n";
                exit(1);
            }

            targetTest = argv[i + 1];
            i++;
        }
        else if (param == "-h" || param == "--help")
        {
            printHelp(argv[0]);
            exit(0);
        }
        else if (param == "-s")
        {
            internal::printSuccessfulTests = true;
        }
        else if (param == "-l")
        {
            listTests = true;
        }
        else
        {
            printErr() << "Unknown parameter: " << param << "\n";
            printHelp(argv[0]);
            exit(1);
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

    std::unique_lock<std::mutex> l(internal::mtxRegister);
    if (internal::registeredTests.empty() == false)
    {
        auto fut = internal::allTestRan.get_future();
        l.unlock();
        fut.get();
        assert(internal::registeredTests.empty());
    }
    testCases.clear();

    printTestStats();

    return internal::numCorrectAssertions != internal::numAssertions;
}

ThreadSafeStream print()
{
    return ThreadSafeStream(std::cout);
}

ThreadSafeStream printErr()
{
    return ThreadSafeStream(std::cerr);
}

}  // namespace test
}  // namespace drogon
