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
    MysqlResultImpl(const std::shared_ptr<MYSQL_RES> &r,
                    const std::string &query,
                    SizeType affectedRows,
                    unsigned long long insertId) noexcept
        : ResultImpl(query),
          _result(r),
          _rowsNum(_result ? mysql_num_rows(_result.get()) : 0),
          _fieldArray(r ? mysql_fetch_fields(r.get()) : nullptr),
          _fieldNum(r ? mysql_num_fields(r.get()) : 0),
          _affectedRows(affectedRows),
          _insertId(insertId)
    {
        if (_fieldNum > 0)
        {
            _fieldMapPtr = std::make_shared<
                std::unordered_map<std::string, RowSizeType>>();
            for (RowSizeType i = 0; i < _fieldNum; i++)
            {
                std::string fieldName = _fieldArray[i].name;
                std::transform(fieldName.begin(),
                               fieldName.end(),
                               fieldName.begin(),
                               tolower);
                (*_fieldMapPtr)[fieldName] = i;
            }
        }
        if (size() > 0)
        {
            _rowsPtr = std::make_shared<
                std::vector<std::pair<char **, std::vector<unsigned long>>>>();
            MYSQL_ROW row;
            std::vector<unsigned long> vLens;
            vLens.resize(_fieldNum);
            while ((row = mysql_fetch_row(r.get())) != NULL)
            {
                auto lengths = mysql_fetch_lengths(r.get());
                memcpy(vLens.data(),
                       lengths,
                       sizeof(unsigned long) * _fieldNum);
                _rowsPtr->push_back(std::make_pair(row, vLens));
            }
        }
    }
    virtual SizeType size() const noexcept override;
    virtual RowSizeType columns() const noexcept override;
    virtual const char *columnName(RowSizeType number) const override;
    virtual SizeType affectedRows() const noexcept override;
    virtual RowSizeType columnNumber(const char colName[]) const override;
    virtual const char *getValue(SizeType row,
                                 RowSizeType column) const override;
    virtual bool isNull(SizeType row, RowSizeType column) const override;
    virtual FieldSizeType getLength(SizeType row,
                                    RowSizeType column) const override;
    virtual unsigned long long insertId() const noexcept override;

  private:
    const std::shared_ptr<MYSQL_RES> _result;
    const Result::SizeType _rowsNum;
    const MYSQL_FIELD *_fieldArray;
    const Result::RowSizeType _fieldNum;
    const SizeType _affectedRows;
    const unsigned long long _insertId;
    std::shared_ptr<std::unordered_map<std::string, RowSizeType>> _fieldMapPtr;
    std::shared_ptr<std::vector<std::pair<char **, std::vector<unsigned long>>>>
        _rowsPtr;
};

}  // namespace orm
}  // namespace drogon
