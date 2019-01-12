/**
 *
 *  MysqlConnection.cc
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

#include "MysqlConnection.h"
#include "MysqlResultImpl.h"
#include <drogon/utils/Utilities.h>
#include <regex>
#include <algorithm>
#include <poll.h>

using namespace drogon::orm;
namespace drogon
{
namespace orm
{

Result makeResult(const std::shared_ptr<MYSQL_RES> &r = std::shared_ptr<MYSQL_RES>(nullptr),
                  const std::string &query = "",
                  Result::size_type affectedRows = 0,
                  unsigned long long insertId = 0)
{
    return Result(std::shared_ptr<MysqlResultImpl>(new MysqlResultImpl(r, query, affectedRows, insertId)));
}

} // namespace orm
} // namespace drogon

MysqlConnection::MysqlConnection(trantor::EventLoop *loop, const std::string &connInfo)
    : DbConnection(loop),
      _mysqlPtr(std::shared_ptr<MYSQL>(new MYSQL, [](MYSQL *p) {
          mysql_close(p);
      }))
{
    mysql_init(_mysqlPtr.get());
    mysql_options(_mysqlPtr.get(), MYSQL_OPT_NONBLOCK, 0);

    //Get the key and value
    std::regex r(" *= *");
    auto tmpStr = std::regex_replace(connInfo, r, "=");
    std::string host, user, passwd, dbname, port;
    auto keyValues = splitString(tmpStr, " ");
    for (auto &kvs : keyValues)
    {
        auto kv = splitString(kvs, "=");
        assert(kv.size() == 2);
        auto key = kv[0];
        auto value = kv[1];
        if (value[0] == '\'' && value[value.length() - 1] == '\'')
        {
            value = value.substr(1, value.length() - 2);
        }
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        //LOG_TRACE << key << "=" << value;
        if (key == "host")
        {
            host = value;
        }
        else if (key == "user")
        {
            user = value;
        }
        else if (key == "dbname")
        {
            dbname = value;
        }
        else if (key == "port")
        {
            port = value;
        }
        else if (key == "password")
        {
            passwd = value;
        }
    }
    _loop->queueInLoop([=]() {
        MYSQL *ret;
        _status = ConnectStatus_Connecting;
        _waitStatus = mysql_real_connect_start(&ret,
                                               _mysqlPtr.get(),
                                               host.empty() ? NULL : host.c_str(),
                                               user.empty() ? NULL : user.c_str(),
                                               passwd.empty() ? NULL : passwd.c_str(),
                                               dbname.empty() ? NULL : dbname.c_str(),
                                               port.empty() ? 3306 : atol(port.c_str()),
                                               NULL,
                                               0);

        auto fd = mysql_get_socket(_mysqlPtr.get());
        _channelPtr = std::unique_ptr<trantor::Channel>(new trantor::Channel(loop, fd));
        _channelPtr->setCloseCallback([=]() {
            perror("sock close");
            handleClosed();
        });
        _channelPtr->setEventCallback([=]() {
            handleEvent();
        });
        setChannel();
    });
}

void MysqlConnection::setChannel()
{
    if ((_waitStatus & MYSQL_WAIT_READ) || (_waitStatus & MYSQL_WAIT_EXCEPT))
    {
        if (!_channelPtr->isReading())
            _channelPtr->enableReading();
    }
    if (_waitStatus & MYSQL_WAIT_WRITE)
    {
        if (!_channelPtr->isWriting())
            _channelPtr->enableWriting();
    }
    else
    {
        if (_channelPtr->isWriting())
            _channelPtr->disableWriting();
    }
    //(status & MYSQL_WAIT_EXCEPT) ///FIXME
    if (_waitStatus & MYSQL_WAIT_TIMEOUT)
    {
        auto timeout = mysql_get_timeout_value(_mysqlPtr.get());
        auto thisPtr = shared_from_this();
        _loop->runAfter(timeout, [thisPtr]() {
            thisPtr->handleTimeout();
        });
    }
}

void MysqlConnection::handleClosed()
{
    _loop->assertInLoopThread();
    if (_status == ConnectStatus_Bad)
        return;
    _status = ConnectStatus_Bad;
    _channelPtr->disableAll();
    _channelPtr->remove();
    assert(_closeCb);
    auto thisPtr = shared_from_this();
    _closeCb(thisPtr);
}
void MysqlConnection::handleTimeout()
{
    LOG_TRACE << "channel index:" << _channelPtr->index();
    int status = 0;
    status |= MYSQL_WAIT_TIMEOUT;
    MYSQL *ret;
    if (_status == ConnectStatus_Connecting)
    {
        _waitStatus = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
        if (_waitStatus == 0)
        {
            if (!ret)
            {
                handleClosed();
                LOG_ERROR << "Failed to mysql_real_connect()";
                return;
            }
            //I don't think the programe can run to here.
            _status = ConnectStatus_Ok;
            if (_okCb)
            {
                auto thisPtr = shared_from_this();
                _okCb(thisPtr);
            }
        }
        setChannel();
    }
    else if (_status == ConnectStatus_Ok)
    {
    }
}
void MysqlConnection::handleEvent()
{
    int status = 0;
    auto revents = _channelPtr->revents();
    if (revents & POLLIN)
        status |= MYSQL_WAIT_READ;
    if (revents & POLLOUT)
        status |= MYSQL_WAIT_WRITE;
    if (revents & POLLPRI)
        status |= MYSQL_WAIT_EXCEPT;
    status = (status & _waitStatus);
    MYSQL *ret;
    if (_status == ConnectStatus_Connecting)
    {
        _waitStatus = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
        if (_waitStatus == 0)
        {
            if (!ret)
            {
                handleClosed();
                //perror("");
                LOG_ERROR << "Failed to mysql_real_connect()";
                return;
            }
            _status = ConnectStatus_Ok;
            if (_okCb)
            {
                auto thisPtr = shared_from_this();
                _okCb(thisPtr);
            }
        }
        setChannel();
    }
    else if (_status == ConnectStatus_Ok)
    {
        switch (_execStatus)
        {
        case ExecStatus_RealQuery:
        {
            int err;
            _waitStatus = mysql_real_query_cont(&err, _mysqlPtr.get(), status);
            LOG_TRACE << "real_query:" << _waitStatus;
            if (_waitStatus == 0)
            {
                if (err)
                {
                    _execStatus = ExecStatus_None;
                    outputError();
                    //FIXME exception callback;
                    return;
                }
                _execStatus = ExecStatus_StoreResult;
                MYSQL_RES *ret;
                _waitStatus = mysql_store_result_start(&ret, _mysqlPtr.get());
                LOG_TRACE << "store_result_start:" << _waitStatus;
                if (_waitStatus == 0)
                {
                    _execStatus = ExecStatus_None;
                    if (err)
                    {
                        outputError();
                        //FIXME exception callback;
                        return;
                    }
                    getResult(ret);
                }
            }
            setChannel();
            break;
        }
        case ExecStatus_StoreResult:
        {
            MYSQL_RES *ret;
            _waitStatus = mysql_store_result_cont(&ret, _mysqlPtr.get(), status);
            LOG_TRACE << "store_result:" << _waitStatus;
            if (_waitStatus == 0)
            {
                if (!ret)
                {
                    _execStatus = ExecStatus_None;
                    outputError();
                    //FIXME exception callback;
                    return;
                }
                getResult(ret);
            }
            setChannel();
            break;
        }
        case ExecStatus_None:
        {
            //Connection closed!
            if (_waitStatus == 0)
                handleClosed();
            break;
        }
        default:
            return;
        }
    }
}

void MysqlConnection::execSql(std::string &&sql,
                              size_t paraNum,
                              std::vector<const char *> &&parameters,
                              std::vector<int> &&length,
                              std::vector<int> &&format,
                              ResultCallback &&rcb,
                              std::function<void(const std::exception_ptr &)> &&exceptCallback,
                              std::function<void()> &&idleCb)
{
    LOG_TRACE << sql;
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(idleCb);
    assert(!_isWorking);
    assert(!sql.empty());

    _cb = std::move(rcb);
    _idleCb = std::move(idleCb);
    _isWorking = true;
    _exceptCb = std::move(exceptCallback);
    _sql.clear();
    if (paraNum > 0)
    {
        std::string::size_type pos = 0;
        std::string::size_type seekPos = std::string::npos;
        for (size_t i = 0; i < paraNum; i++)
        {
            seekPos = sql.find("?", pos);
            if (seekPos == std::string::npos)
            {
                _sql.append(sql.substr(pos));
                pos = seekPos;
                break;
            }
            else
            {
                _sql.append(sql.substr(pos, seekPos - pos));
                pos = seekPos + 1;
                switch (format[i])
                {
                case MYSQL_TYPE_TINY:
                    _sql.append(std::to_string(*((char *)parameters[i])));
                    break;
                case MYSQL_TYPE_SHORT:
                    _sql.append(std::to_string(*((short *)parameters[i])));
                    break;
                case MYSQL_TYPE_LONG:
                    _sql.append(std::to_string(*((int32_t *)parameters[i])));
                    break;
                case MYSQL_TYPE_LONGLONG:
                    _sql.append(std::to_string(*((int64_t *)parameters[i])));
                    break;
                case MYSQL_TYPE_NULL:
                    _sql.append("NULL");
                    break;
                case MYSQL_TYPE_STRING:
                {
                    _sql.append("'");
                    std::string to(length[i] * 2, '\0');
                    auto len = mysql_real_escape_string(_mysqlPtr.get(), (char *)to.c_str(), parameters[i], length[i]);
                    to.resize(len);
                    _sql.append(to);
                    _sql.append("'");
                }
                break;
                default:
                    break;
                }
            }
        }
        if (pos < sql.length())
        {
            _sql.append(sql.substr(pos));
        }
    }
    else
    {
        _sql = sql;
    }
    LOG_TRACE << _sql;
    _loop->runInLoop([=]() {
        int err;
        //int mysql_real_query_start(int *ret, MYSQL *mysql, const char *q, unsigned long length)
        _waitStatus = mysql_real_query_start(&err, _mysqlPtr.get(), _sql.c_str(), _sql.length());
        LOG_TRACE << "real_query:" << _waitStatus;
        _execStatus = ExecStatus_RealQuery;
        if (_waitStatus == 0)
        {
            if (err)
            {
                outputError();
                return;
            }

            MYSQL_RES *ret;
            _waitStatus = mysql_store_result_start(&ret, _mysqlPtr.get());
            LOG_TRACE << "store_result:" << _waitStatus;
            _execStatus = ExecStatus_StoreResult;
            if (_waitStatus == 0)
            {
                _execStatus = ExecStatus_None;
                if (!ret)
                {
                    outputError();
                    return;
                }
                getResult(ret);
            }
        }
        setChannel();
    });
    return;
}

void MysqlConnection::outputError()
{
    _channelPtr->disableAll();
    LOG_ERROR << "Error("
              << mysql_errno(_mysqlPtr.get()) << ") [" << mysql_sqlstate(_mysqlPtr.get()) << "] \""
              << mysql_error(_mysqlPtr.get()) << "\"";
    if (_isWorking)
    {

        try
        {
            //FIXME exception type
            throw SqlError(mysql_error(_mysqlPtr.get()),
                           _sql);
        }
        catch (...)
        {
            _exceptCb(std::current_exception());
            _exceptCb = decltype(_exceptCb)();
        }

        _cb = decltype(_cb)();
        _isWorking = false;
        if (_idleCb)
        {
            _idleCb();
            _idleCb = decltype(_idleCb)();
        }
    }
}

void MysqlConnection::getResult(MYSQL_RES *res)
{
    auto resultPtr = std::shared_ptr<MYSQL_RES>(res, [](MYSQL_RES *r) {
        mysql_free_result(r);
    });
    auto Result = makeResult(resultPtr, _sql, mysql_affected_rows(_mysqlPtr.get()), mysql_insert_id(_mysqlPtr.get()));
    if (_isWorking)
    {
        _cb(Result);
        _cb = decltype(_cb)();
        _exceptCb = decltype(_exceptCb)();
        _isWorking = false;
        _idleCb();
        _idleCb = decltype(_idleCb)();
    }
}
