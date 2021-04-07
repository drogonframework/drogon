/**
 *
 *  @file Logger.h
 *  @author An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#pragma once

#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/Date.h>
#include <trantor/utils/LogStream.h>
#include <trantor/exports.h>
#include <string.h>
#include <functional>
#include <iostream>

namespace trantor
{
/**
 * @brief This class implements log functions.
 *
 */
class TRANTOR_EXPORT Logger : public NonCopyable
{
  public:
    enum LogLevel
    {
        kTrace = 0,
        kDebug,
        kInfo,
        kWarn,
        kError,
        kFatal,
        kNumberOfLogLevels
    };

    /**
     * @brief Calculate of basename of source files in compile time.
     *
     */
    class SourceFile
    {
      public:
        template <int N>
        inline SourceFile(const char (&arr)[N]) : data_(arr), size_(N - 1)
        {
            // std::cout<<data_<<std::endl;
            const char *slash = strrchr(data_, '/');  // builtin function
            if (slash)
            {
                data_ = slash + 1;
                size_ -= static_cast<int>(data_ - arr);
            }
        }

        explicit SourceFile(const char *filename) : data_(filename)
        {
            const char *slash = strrchr(filename, '/');
            if (slash)
            {
                data_ = slash + 1;
            }
            size_ = static_cast<int>(strlen(data_));
        }

        const char *data_;
        int size_;
    };
    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, bool isSysErr);
    Logger(SourceFile file, int line, LogLevel level, const char *func);
    ~Logger();
    LogStream &stream();

    /**
     * @brief Set the output function.
     *
     * @param outputFunc The function to output a log message.
     * @param flushFunc The function to flush.
     * @note Logs are output to the standard output by default.
     */
    static void setOutputFunction(
        std::function<void(const char *msg, const uint64_t len)> outputFunc,
        std::function<void()> flushFunc)
    {
        outputFunc_() = outputFunc;
        flushFunc_() = flushFunc;
    }

    /**
     * @brief Set the log level. Logs below the level are not printed.
     *
     * @param level
     */
    static void setLogLevel(LogLevel level)
    {
        logLevel_() = level;
    }

    /**
     * @brief Get the current log level.
     *
     * @return LogLevel
     */
    static LogLevel logLevel()
    {
        return logLevel_();
    }

  protected:
    static void defaultOutputFunction(const char *msg, const uint64_t len)
    {
        fwrite(msg, 1, len, stdout);
    }
    static void defaultFlushFunction()
    {
        fflush(stdout);
    }
    void formatTime();
    static LogLevel &logLevel_()
    {
#ifdef RELEASE
        static LogLevel logLevel = LogLevel::kInfo;
#else
        static LogLevel logLevel = LogLevel::kDebug;
#endif
        return logLevel;
    }
    static std::function<void(const char *msg, const uint64_t len)>
        &outputFunc_()
    {
        static std::function<void(const char *msg, const uint64_t len)>
            outputFunc = Logger::defaultOutputFunction;
        return outputFunc;
    }
    static std::function<void()> &flushFunc_()
    {
        static std::function<void()> flushFunc = Logger::defaultFlushFunction;
        return flushFunc;
    }
    LogStream logStream_;
    Date date_{Date::now()};
    SourceFile sourceFile_;
    int fileLine_;
    LogLevel level_;
};
#ifdef NDEBUG
#define LOG_TRACE                                                          \
    if (0)                                                                 \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kTrace, __func__) \
        .stream()
#else
#define LOG_TRACE                                                          \
    if (trantor::Logger::logLevel() <= trantor::Logger::kTrace)            \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kTrace, __func__) \
        .stream()
#endif
#define LOG_DEBUG                                                          \
    if (trantor::Logger::logLevel() <= trantor::Logger::kDebug)            \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kDebug, __func__) \
        .stream()
#define LOG_INFO                                               \
    if (trantor::Logger::logLevel() <= trantor::Logger::kInfo) \
    trantor::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kWarn).stream()
#define LOG_ERROR \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kError).stream()
#define LOG_FATAL \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kFatal).stream()
#define LOG_SYSERR trantor::Logger(__FILE__, __LINE__, true).stream()

#define LOG_TRACE_IF(cond)                                                  \
    if ((trantor::Logger::logLevel() <= trantor::Logger::kTrace) && (cond)) \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kTrace, __func__)  \
        .stream()
#define LOG_DEBUG_IF(cond)                                                \
    if ((Tensor::Logger::logLevel() <= Tensor::Logger::kDebug) && (cond)) \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kDebug, __func__)  \
        .stream()
#define LOG_INFO_IF(cond)                                                \
    if ((Tensor::Logger::logLevel() <= Tensor::Logger::kInfo) && (cond)) \
    Tensor::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN_IF(cond) \
    if (cond)             \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kWarn).stream()
#define LOG_ERROR_IF(cond) \
    if (cond)              \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kError).stream()
#define LOG_FATAL_IF(cond) \
    if (cond)              \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kFatal).stream()

#ifdef NDEBUG
#define DLOG_TRACE                                                         \
    if (0)                                                                 \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kTrace, __func__) \
        .stream()
#define DLOG_DEBUG                                                       \
    if (0)                                                               \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kDebug, __func__) \
        .stream()
#define DLOG_INFO \
    if (0)        \
    Tensor::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN \
    if (0)        \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kWarn).stream()
#define DLOG_ERROR \
    if (0)         \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kError).stream()
#define DLOG_FATAL \
    if (0)         \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kFatal).stream()

#define DLOG_TRACE_IF(cond)                                                \
    if (0)                                                                 \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kTrace, __func__) \
        .stream()
#define DLOG_DEBUG_IF(cond)                                              \
    if (0)                                                               \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kDebug, __func__) \
        .stream()
#define DLOG_INFO_IF(cond) \
    if (0)                 \
    Tensor::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN_IF(cond) \
    if (0)                 \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kWarn).stream()
#define DLOG_ERROR_IF(cond) \
    if (0)                  \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kError).stream()
#define DLOG_FATAL_IF(cond) \
    if (0)                  \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kFatal).stream()
#else
#define DLOG_TRACE                                                         \
    if (trantor::Logger::logLevel() <= trantor::Logger::kTrace)            \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kTrace, __func__) \
        .stream()
#define DLOG_DEBUG                                                       \
    if (Tensor::Logger::logLevel() <= Tensor::Logger::kDebug)            \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kDebug, __func__) \
        .stream()
#define DLOG_INFO                                            \
    if (Tensor::Logger::logLevel() <= Tensor::Logger::kInfo) \
    Tensor::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kWarn).stream()
#define DLOG_ERROR \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kError).stream()
#define DLOG_FATAL \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kFatal).stream()

#define DLOG_TRACE_IF(cond)                                                 \
    if ((trantor::Logger::logLevel() <= trantor::Logger::kTrace) && (cond)) \
    trantor::Logger(__FILE__, __LINE__, trantor::Logger::kTrace, __func__)  \
        .stream()
#define DLOG_DEBUG_IF(cond)                                               \
    if ((Tensor::Logger::logLevel() <= Tensor::Logger::kDebug) && (cond)) \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kDebug, __func__)  \
        .stream()
#define DLOG_INFO_IF(cond)                                               \
    if ((Tensor::Logger::logLevel() <= Tensor::Logger::kInfo) && (cond)) \
    Tensor::Logger(__FILE__, __LINE__).stream()
#define DLOG_WARN_IF(cond) \
    if (cond)              \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kWarn).stream()
#define DLOG_ERROR_IF(cond) \
    if (cond)               \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kError).stream()
#define DLOG_FATAL_IF(cond) \
    if (cond)               \
    Tensor::Logger(__FILE__, __LINE__, Tensor::Logger::kFatal).stream()
#endif

const char *strerror_tl(int savedErrno);
}  // namespace trantor
