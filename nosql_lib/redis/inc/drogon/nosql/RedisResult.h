/**
 *
 *  @file RedisResult.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/exports.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

struct redisReply;
namespace drogon
{
namespace nosql
{
enum class RedisResultType
{
    kInteger = 0,
    kString,
    kArray,
    kStatus,
    kNil,
    kError
};

/**
 * @brief This class represents a redis reply with no error.
 * @note Limited by the hiredis library, the RedisResult object is only
 * available in the context of the result callback, one can't hold or copy or
 * move a RedisResult object for later use after the callback is returned.
 */
class DROGON_EXPORT RedisResult
{
  public:
    explicit RedisResult(redisReply *result) : result_(result)
    {
    }
    ~RedisResult() = default;

    /**
     * @brief Return the type of the result_
     * @return RedisResultType
     * @note The kError type is never obtained here.
     */
    RedisResultType type() const noexcept;

    /**
     * @brief Get the string value of the result.
     *
     * @return std::string
     * @note Calling the method of a result object which is the kArray type
     * throws a runtime exception.
     */
    std::string asString() const noexcept(false);

    /**
     * @brief Get the array value of the result.
     *
     * @return std::vector<RedisResult>
     * @note Calling the method of a result object whose type is not kArray type
     * throws a runtime exception.
     */
    std::vector<RedisResult> asArray() const noexcept(false);

    /**
     * @brief Get the integer value of the result.
     *
     * @return long long
     * @note Calling the method of a result object whose type is not kInteger
     * type throws a runtime exception.
     */
    long long asInteger() const noexcept(false);

    /**
     * @brief Get the string for displaying the result.
     *
     * @return std::string
     */
    std::string getStringForDisplaying() const noexcept;

    /**
     * @brief Get the string for displaying with indent.
     *
     * @param indent The indent value.
     * @return std::string
     */
    std::string getStringForDisplayingWithIndent(
        size_t indent = 0) const noexcept;

    /**
     * @brief return true if the result object is nil.
     *
     * @return true
     * @return false
     */
    bool isNil() const noexcept;

    /**
     * @brief Check if the result object is not nil.
     *
     * @return true
     * @return false
     */
    explicit operator bool() const
    {
        return !isNil();
    }

  private:
    redisReply *result_;
};

using RedisResultCallback = std::function<void(const RedisResult &)>;
using RedisMessageCallback =
    std::function<void(const std::string &channel, const std::string &message)>;

}  // namespace nosql
}  // namespace drogon
