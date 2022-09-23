/**
 *
 *  MysqlResultImpl.h
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

#include "../ResultImpl.h"
#include <trantor/utils/Logger.h>
#include <algorithm>
#include <memory>
#include <mysql.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace drogon
{
namespace orm
{
class MysqlResultImpl : public ResultImpl
{
  public:
    MysqlResultImpl(std::shared_ptr<MYSQL_RES> r,
                    SizeType affectedRows,
                    unsigned long long insertId) noexcept
        : result_(std::move(r)),
          rowsNumber_(result_ ? mysql_num_rows(result_.get()) : 0),
          fieldArray_(result_ ? mysql_fetch_fields(result_.get()) : nullptr),
          fieldsNumber_(result_ ? mysql_num_fields(result_.get()) : 0),
          affectedRows_(affectedRows),
          insertId_(insertId)
    {
        if (fieldsNumber_ > 0)
        {
            fieldsMapPtr_ = std::make_shared<
                std::unordered_map<std::string, RowSizeType>>();
            for (RowSizeType i = 0; i < fieldsNumber_; ++i)
            {
                std::string fieldName = fieldArray_[i].name;
                std::transform(fieldName.begin(),
                               fieldName.end(),
                               fieldName.begin(),
                               [](unsigned char c) { return tolower(c); });
                (*fieldsMapPtr_)[fieldName] = i;
            }
        }
        if (size() > 0)
        {
            rowsPtr_ = std::make_shared<
                std::vector<std::pair<char **, std::vector<unsigned long>>>>();
            MYSQL_ROW row;
            std::vector<unsigned long> vLens;
            vLens.resize(fieldsNumber_);
            while ((row = mysql_fetch_row(result_.get())) != NULL)
            {
                auto lengths = mysql_fetch_lengths(result_.get());
                memcpy(vLens.data(),
                       lengths,
                       sizeof(unsigned long) * fieldsNumber_);
                rowsPtr_->push_back(std::make_pair(row, vLens));
            }
        }
    }
    SizeType size() const noexcept override;
    RowSizeType columns() const noexcept override;
    const char *columnName(RowSizeType number) const override;
    SizeType affectedRows() const noexcept override;
    RowSizeType columnNumber(const char colName[]) const override;
    const char *getValue(SizeType row, RowSizeType column) const override;
    bool isNull(SizeType row, RowSizeType column) const override;
    FieldSizeType getLength(SizeType row, RowSizeType column) const override;
    unsigned long long insertId() const noexcept override;

  private:
    const std::shared_ptr<MYSQL_RES> result_;
    const Result::SizeType rowsNumber_;
    const MYSQL_FIELD *fieldArray_;
    const Result::RowSizeType fieldsNumber_;
    const SizeType affectedRows_;
    const unsigned long long insertId_;
    std::shared_ptr<std::unordered_map<std::string, RowSizeType>> fieldsMapPtr_;
    std::shared_ptr<std::vector<std::pair<char **, std::vector<unsigned long>>>>
        rowsPtr_;
};

}  // namespace orm
}  // namespace drogon
