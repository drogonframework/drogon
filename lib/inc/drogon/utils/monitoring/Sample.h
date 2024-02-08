/**
 *
 *  Sample.h
 *  An Tao
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
#include <trantor/utils/Date.h>
#include <vector>
#include <string>

namespace drogon
{
namespace monitoring
{
/**
 * This class is used to collect samples for a metric.
 * */
struct Sample
{
    double value{0};
    trantor::Date timestamp{0};
    std::string name;
    std::vector<std::pair<std::string, std::string>> exLabels;
};
}  // namespace monitoring
}  // namespace drogon
