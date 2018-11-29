/**
 *
 *  MysqlStmtResultImpl.h
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
#include <trantor/utils/Logger.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <string.h>

namespace drogon
{
namespace orm
{

class MysqlStmtResultImpl : public ResultImpl
{
  public:
    MysqlStmtResultImpl(const std::shared_ptr<MYSQL_STMT> &r, const std::string &query) noexcept
        : _result(r),
          _metaData(mysql_stmt_result_metadata(r.get()), [](MYSQL_RES *p) {
        if (p)
            mysql_free_result(p); }),
          _query(query),
          _rowsNum(r ? mysql_stmt_num_rows(r.get()) : 0),
          _fieldArray(_metaData ? mysql_fetch_fields(_metaData.get()) : nullptr),
          _fieldNum(_metaData ? mysql_num_fields(_metaData.get()) : 0),
          _affectedRows(r ? mysql_stmt_affected_rows(r.get()) : 0)
    {
        MYSQL_BIND binds[_fieldNum];
        memset(binds, 0, sizeof(MYSQL_BIND) * _fieldNum);
        unsigned long lengths[_fieldNum];
        my_bool isNulls[_fieldNum];
        std::shared_ptr<char> buffers[_fieldNum];
        char fakeBuf;
        if (_fieldNum > 0)
        {
            for (row_size_type i = 0; i < _fieldNum; i++)
            {
                std::string fieldName = _fieldArray[i].name;
                std::transform(fieldName.begin(), fieldName.end(), fieldName.begin(), tolower);
                _fieldMap[fieldName] = i;
                LOG_TRACE << "row[" << fieldName << "].max_length=" << _fieldArray[i].max_length;
                if (_rowsNum > 0)
                {
                    if (_fieldArray[i].max_length > 0)
                    {
                        buffers[i] = std::shared_ptr<char>(new char[_fieldArray[i].max_length + 1], [](char *p) { delete[] p; });
                        binds[i].buffer = buffers[i].get();
                        binds[i].buffer_length = _fieldArray[i].max_length + 1;
                    }
                    else
                    {
                        binds[i].buffer = &fakeBuf;
                        binds[i].buffer_length = 1;
                    }
                    binds[i].length = &lengths[i];
                    binds[i].is_null = &isNulls[i];
                    binds[i].buffer_type = _fieldArray[i].type;
                }
            }
        }
        if (size() > 0)
        {
            if (mysql_stmt_bind_result(r.get(), binds))
            {
                fprintf(stderr, " mysql_stmt_bind_result() failed\n");
                fprintf(stderr, " %s\n", mysql_stmt_error(r.get()));
                exit(-1);
            }
            while (!mysql_stmt_fetch(r.get()))
            {
                std::vector<char *> row;
                std::vector<field_size_type> lengths;
                for (row_size_type i = 0; i < _fieldNum; i++)
                {
                    if (*(binds[i].is_null))
                    {
                        row.push_back(NULL);
                        lengths.push_back(0);
                    }
                    else
                    {
                        auto data = std::shared_ptr<char>(new char[*(binds[i].length)], [](char *p) { delete[] p; });
                        memcpy(data.get(), binds[i].buffer, *binds[i].length);
                        _resultData.push_back(data);
                        row.push_back(data.get());
                        lengths.push_back(*(binds[i].length));
                    }
                }
                _rows.push_back(std::make_pair(row, lengths));
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
    const std::shared_ptr<MYSQL_STMT> _result;
    const std::shared_ptr<MYSQL_RES> _metaData;
    const std::string _query;
    const Result::size_type _rowsNum;
    const MYSQL_FIELD *_fieldArray;
    const Result::row_size_type _fieldNum;
    const size_type _affectedRows;
    std::unordered_map<std::string, row_size_type> _fieldMap;
    std::vector<std::pair<std::vector<char *>, std::vector<field_size_type>>> _rows;

    std::vector<std::shared_ptr<char>> _resultData;
};

} // namespace orm
} // namespace drogon
