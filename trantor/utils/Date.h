/**
 *
 *  @file Date.h
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

#include <trantor/exports.h>
#include <stdint.h>
#include <string>

#define MICRO_SECONDS_PRE_SEC 1000000

namespace trantor
{
/**
 * @brief This class represents a time point.
 *
 */
class TRANTOR_EXPORT Date
{
  public:
    Date() : microSecondsSinceEpoch_(0){};

    /**
     * @brief Construct a new Date instance.
     *
     * @param microSec The microseconds from 1970-01-01 00:00:00.
     */
    explicit Date(int64_t microSec) : microSecondsSinceEpoch_(microSec){};

    /**
     * @brief Construct a new Date instance.
     *
     * @param year
     * @param month
     * @param day
     * @param hour
     * @param minute
     * @param second
     * @param microSecond
     */
    Date(unsigned int year,
         unsigned int month,
         unsigned int day,
         unsigned int hour = 0,
         unsigned int minute = 0,
         unsigned int second = 0,
         unsigned int microSecond = 0);

    /**
     * @brief Create a Date object that represents the current time.
     *
     * @return const Date
     */
    static const Date date();

    /**
     * @brief Same as the date() method.
     *
     * @return const Date
     */
    static const Date now()
    {
        return Date::date();
    }

    /**
     * @brief Return a new Date instance that represents the time after some
     * seconds from *this.
     *
     * @param second
     * @return const Date
     */
    const Date after(double second) const;

    /**
     * @brief Return a new Date instance that equals to *this, but with zero
     * microseconds.
     *
     * @return const Date
     */
    const Date roundSecond() const;

    /// Create a Date object equal to * this, but numbers of hours, minutes,
    /// seconds and microseconds are zero.

    /**
     * @brief Return a new Date instance that equals to * this, but with zero
     * hours, minutes, seconds and microseconds.
     *
     * @return const Date
     */
    const Date roundDay() const;

    ~Date(){};

    /**
     * @brief Return true if the time point is equal to another.
     *
     */
    bool operator==(const Date &date) const
    {
        return microSecondsSinceEpoch_ == date.microSecondsSinceEpoch_;
    }

    /**
     * @brief Return true if the time point is not equal to another.
     *
     */
    bool operator!=(const Date &date) const
    {
        return microSecondsSinceEpoch_ != date.microSecondsSinceEpoch_;
    }

    /**
     * @brief Return true if the time point is earlier than another.
     *
     */
    bool operator<(const Date &date) const
    {
        return microSecondsSinceEpoch_ < date.microSecondsSinceEpoch_;
    }

    /**
     * @brief Return true if the time point is later than another.
     *
     */
    bool operator>(const Date &date) const
    {
        return microSecondsSinceEpoch_ > date.microSecondsSinceEpoch_;
    }

    /**
     * @brief Return true if the time point is not earlier than another.
     *
     */
    bool operator>=(const Date &date) const
    {
        return microSecondsSinceEpoch_ >= date.microSecondsSinceEpoch_;
    }

    /**
     * @brief Return true if the time point is not later than another.
     *
     */
    bool operator<=(const Date &date) const
    {
        return microSecondsSinceEpoch_ <= date.microSecondsSinceEpoch_;
    }

    /**
     * @brief Get the number of milliseconds since 1970-01-01 00:00.
     *
     * @return int64_t
     */
    int64_t microSecondsSinceEpoch() const
    {
        return microSecondsSinceEpoch_;
    }

    /**
     * @brief Get the number of seconds since 1970-01-01 00:00.
     *
     * @return int64_t
     */
    int64_t secondsSinceEpoch() const
    {
        return microSecondsSinceEpoch_ / MICRO_SECONDS_PRE_SEC;
    }

    /**
     * @brief Get the tm struct for the time point.
     *
     * @return struct tm
     */
    struct tm tmStruct() const;

    /**
     * @brief Generate a UTC time string
     * @example:
     * 20180101 10:10:25            //If the @param showMicroseconds is false
     * 20180101 10:10:25:102414     //If the @param showMicroseconds is true
     */
    std::string toFormattedString(bool showMicroseconds) const;

    /**
     * @brief Generate a UTC time string formated by the @param fmtStr
     * The @param fmtStr is the format string for the function strftime()
     * @example:
     * 2018-01-01 10:10:25          //If the @param fmtStr is "%Y-%m-%d
     * %H:%M:%S" and the @param showMicroseconds is false 2018-01-01
     * 10:10:25:102414   //If the @param fmtStr is "%Y-%m-%d %H:%M:%S" and the
     * @param showMicroseconds is true
     */
    std::string toCustomedFormattedString(const std::string &fmtStr,
                                          bool showMicroseconds = false) const;

    /**
     * @brief Generate a local time zone string, the format of the string is
     * same as the mothed toFormattedString
     *
     * @param showMicroseconds
     * @return std::string
     */
    std::string toFormattedStringLocal(bool showMicroseconds) const;

    /**
     * @brief Generate a local time zone string formated by the @param fmtStr
     *
     * @param fmtStr
     * @param showMicroseconds
     * @return std::string
     */
    std::string toCustomedFormattedStringLocal(
        const std::string &fmtStr,
        bool showMicroseconds = false) const;

    /**
     * @brief Generate a local time zone string for database.
     * @example:
     * 2018-01-01                   //If hours, minutes, seconds and
     * microseconds are zero 2018-01-01 10:10:25          //If the microsecond
     * is zero 2018-01-01 10:10:25:102414   //If the microsecond is not zero
     */
    std::string toDbStringLocal() const;

    /**
     * @brief From DB string to trantor local time zone.
     *
     * Inverse of toDbStringLocal()
     */
    static Date fromDbStringLocal(const std::string &datetime);

    /**
     * @brief Generate a UTC time string.
     *
     * @param fmtStr The format string.
     * @param str The string buffer for the generated time string.
     * @param len The length of the string buffer.
     */
    void toCustomedFormattedString(const std::string &fmtStr,
                                   char *str,
                                   size_t len) const;  // UTC

    /**
     * @brief Return true if the time point is in a same second as another.
     *
     * @param date
     * @return true
     * @return false
     */
    bool isSameSecond(const Date &date) const
    {
        return microSecondsSinceEpoch_ / MICRO_SECONDS_PRE_SEC ==
               date.microSecondsSinceEpoch_ / MICRO_SECONDS_PRE_SEC;
    }

    /**
     * @brief Swap the time point with another.
     *
     * @param that
     */
    void swap(Date &that)
    {
        std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
    }

  private:
    int64_t microSecondsSinceEpoch_{0};
};
}  // namespace trantor
