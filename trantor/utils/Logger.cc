/**
 *
 *  Logger.cc
 *  An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#include <trantor/utils/Logger.h>
#include <stdio.h>
#include <thread>
#ifndef _WIN32
#include <unistd.h>
#include <sys/syscall.h>
#else
#include <sstream>
#endif
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif

namespace trantor
{
// helper class for known string length at compile time
class T
{
  public:
    T(const char *str, unsigned len) : str_(str), len_(len)
    {
        assert(strlen(str) == len_);
    }

    const char *str_;
    const unsigned len_;
};

const char *strerror_tl(int savedErrno)
{
#ifndef _MSC_VER
    return strerror(savedErrno);
#else
    static thread_local char errMsg[64];
    (void)strerror_s<sizeof errMsg>(errMsg, savedErrno);
    return errMsg;
#endif
}

inline LogStream &operator<<(LogStream &s, T v)
{
    s.append(v.str_, v.len_);
    return s;
}

inline LogStream &operator<<(LogStream &s, const Logger::SourceFile &v)
{
    s.append(v.data_, v.size_);
    return s;
}
}  // namespace trantor
using namespace trantor;

static thread_local uint64_t lastSecond_{0};
static thread_local char lastTimeString_[32] = {0};
#ifdef __linux__
static thread_local pid_t threadId_{0};
#else
static thread_local uint64_t threadId_{0};
#endif
//   static thread_local LogStream logStream_;

void Logger::formatTime()
{
    uint64_t now = static_cast<uint64_t>(date_.secondsSinceEpoch());
    uint64_t microSec =
        static_cast<uint64_t>(date_.microSecondsSinceEpoch() -
                              date_.roundSecond().microSecondsSinceEpoch());
    if (now != lastSecond_)
    {
        lastSecond_ = now;
#ifndef _MSC_VER
        strncpy(lastTimeString_,
                date_.toFormattedString(false).c_str(),
                sizeof(lastTimeString_) - 1);
#else
        strncpy_s<sizeof lastTimeString_>(
            lastTimeString_,
            date_.toFormattedString(false).c_str(),
            sizeof(lastTimeString_) - 1);
#endif
    }
    logStream_ << T(lastTimeString_, 17);
    char tmp[32];
    snprintf(tmp,
             sizeof(tmp),
             ".%06llu UTC ",
             static_cast<long long unsigned int>(microSec));
    logStream_ << T(tmp, 12);
#ifdef __linux__
    if (threadId_ == 0)
        threadId_ = static_cast<pid_t>(::syscall(SYS_gettid));
#elif defined __FreeBSD__
    if (threadId_ == 0)
    {
        threadId_ = pthread_getthreadid_np();
    }
#elif defined __OpenBSD__
    if (threadId_ == 0)
    {
        threadId_ = getthrid();
    }
#elif defined _WIN32
    if (threadId_ == 0)
    {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        threadId_ = std::stoull(ss.str());
    }
#else
    if (threadId_ == 0)
    {
        pthread_threadid_np(NULL, &threadId_);
    }
#endif
    logStream_ << threadId_;
}
static const char *logLevelStr[Logger::LogLevel::kNumberOfLogLevels] = {
    " TRACE ",
    " DEBUG ",
    " INFO  ",
    " WARN  ",
    " ERROR ",
    " FATAL ",
};
Logger::Logger(SourceFile file, int line)
    : sourceFile_(file), fileLine_(line), level_(kInfo)
{
    formatTime();
    logStream_ << T(logLevelStr[level_], 7);
}
Logger::Logger(SourceFile file, int line, LogLevel level)
    : sourceFile_(file), fileLine_(line), level_(level)
{
    formatTime();
    logStream_ << T(logLevelStr[level_], 7);
}
Logger::Logger(SourceFile file, int line, LogLevel level, const char *func)
    : sourceFile_(file), fileLine_(line), level_(level)
{
    formatTime();
    logStream_ << T(logLevelStr[level_], 7) << "[" << func << "] ";
}
Logger::Logger(SourceFile file, int line, bool)
    : sourceFile_(file), fileLine_(line), level_(kFatal)
{
    formatTime();
    logStream_ << T(logLevelStr[level_], 7);
    if (errno != 0)
    {
        logStream_ << strerror_tl(errno) << " (errno=" << errno << ") ";
    }
}
Logger::~Logger()
{
    logStream_ << T(" - ", 3) << sourceFile_ << ':' << fileLine_ << '\n';
    auto oFunc = Logger::outputFunc_();
    if (!oFunc)
        return;
    oFunc(logStream_.bufferData(), logStream_.bufferLength());
    if (level_ >= kError)
        Logger::flushFunc_()();
    // logStream_.resetBuffer();
}
LogStream &Logger::stream()
{
    return logStream_;
}
