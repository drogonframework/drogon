/**
 *
 *  MysqlResultImpl.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */
#pragma once

#include "../ResultImpl.h"

#include <mysql.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>

namespace drogon
{
namespace orm
{

class MysqlResultImpl : public ResultImpl
{
  public:
    MysqlResultImpl(const std::shared_ptr<MYSQL_RES> &r, const std::string &query, size_type affectedRows) noexcept
        : _result(r),
          _query(query),
          _rowsNum(_result ? mysql_num_rows(_result.get()) : 0),
          _fieldArray(r ? mysql_fetch_fields(r.get()) : nullptr),
          _fieldNum(r ? mysql_num_rows(r.get()) : 0),
          _affectedRows(affectedRows)
    {
        if (_fieldNum > 0)
        {
            _fieldMapPtr = std::make_shared<std::unordered_map<std::string, row_size_type>>();
            for (row_size_type i = 0; i < _fieldNum; i++)
            {
                std::string fieldName = _fieldArray[i].name;
                std::transform(fieldName.begin(), fieldName.end(), fieldName.begin(), tolower);
                (*_fieldMapPtr)[fieldName] = i;
            }
        }
        if (size() > 0)
        {
            _rowsPtr = std::make_shared<std::vector<std::pair<char **, field_size_type *>>>();
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(r.get())) != NULL)
            {
                auto lengths = mysql_fetch_lengths(r.get());
                _rowsPtr->push_back(std::make_pair(row, lengths));
            }
        }
    }
    virtual size_type size() const noexcept override;
    virtual row_size_type columns() const noexcept override;
    virtual const char *columnName(row_size_type Number) const override;
    virtual size_type affectedRows() const noexcept override;
    virtual row_size_type columnNumber(const char colName[]) const override;
    virtual const char *getValue(size_type row, row_size_type column) const override;
    virtual bool isNull(size_type row, row_size_type column) const override;
    virtual field_size_type getLength(size_type row, row_size_type column) const override;

  private:
    const std::shared_ptr<MYSQL_RES> _result;
    const std::string _query;
    const Result::size_type _rowsNum;
    const MYSQL_FIELD *_fieldArray;
    const Result::row_size_type _fieldNum;
    const size_type _affectedRows;
    std::shared_ptr<std::unordered_map<std::string, row_size_type>> _fieldMapPtr;
    std::shared_ptr<std::vector<std::pair<char **, field_size_type *>>> _rowsPtr;
};

} // namespace orm
} // namespace drogon
