/**
 *
 *  @file MysqlConnection.cc
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

#include "MysqlConnection.h"
#include "MysqlResultImpl.h"
#include <algorithm>
#include <drogon/utils/Utilities.h>
#include <drogon/utils/string_view.h>
#include <errmsg.h>
#ifndef _WIN32
#include <poll.h>
#else
#define POLLIN (1U << 0)
#define POLLPRI (1U << 1)
#define POLLOUT (1U << 2)
#endif
#include <regex>

using namespace drogon;
using namespace drogon::orm;
namespace drogon
{
namespace orm
{
Result makeResult(
    const std::shared_ptr<MYSQL_RES> &r = std::shared_ptr<MYSQL_RES>(nullptr),
    Result::SizeType affectedRows = 0,
    unsigned long long insertId = 0)
{
    return Result(std::shared_ptr<MysqlResultImpl>(
        new MysqlResultImpl(r, affectedRows, insertId)));
}

}  // namespace orm
}  // namespace drogon

MysqlConnection::MysqlConnection(trantor::EventLoop *loop,
                                 const std::string &connInfo)
    : DbConnection(loop),
      mysqlPtr_(std::shared_ptr<MYSQL>(new MYSQL, [](MYSQL *p) {
          mysql_close(p);
          delete p;
      }))
{
    mysql_init(mysqlPtr_.get());
    mysql_options(mysqlPtr_.get(), MYSQL_OPT_NONBLOCK, 0);

    // Get the key and value
    auto connParams = parseConnString(connInfo);
    for (auto const &kv : connParams)
    {
        auto key = kv.first;
        auto value = kv.second;

        std::transform(key.begin(), key.end(), key.begin(), tolower);
        // LOG_TRACE << key << "=" << value;
        if (key == "host")
        {
            host_ = value;
        }
        else if (key == "user")
        {
            user_ = value;
        }
        else if (key == "dbname")
        {
            // LOG_DEBUG << "database:[" << value << "]";
            dbname_ = value;
        }
        else if (key == "port")
        {
            port_ = value;
        }
        else if (key == "password")
        {
            passwd_ = value;
        }
        else if (key == "client_encoding")
        {
            characterSet_ = value;
        }
    }
    loop_->queueInLoop([this]() {
        MYSQL *ret;
        status_ = ConnectStatus::Connecting;
        waitStatus_ =
            mysql_real_connect_start(&ret,
                                     mysqlPtr_.get(),
                                     host_.empty() ? NULL : host_.c_str(),
                                     user_.empty() ? NULL : user_.c_str(),
                                     passwd_.empty() ? NULL : passwd_.c_str(),
                                     dbname_.empty() ? NULL : dbname_.c_str(),
                                     port_.empty() ? 3306 : atol(port_.c_str()),
                                     NULL,
                                     0);
        // LOG_DEBUG << ret;
        auto fd = mysql_get_socket(mysqlPtr_.get());
        if (fd < 0)
        {
            LOG_FATAL << "Socket fd < 0, Usually this is because the number of "
                         "files opened by the program exceeds the system "
                         "limit. Please use the ulimit command to check.";
            exit(1);
        }
        channelPtr_ =
            std::unique_ptr<trantor::Channel>(new trantor::Channel(loop_, fd));
        channelPtr_->setEventCallback([this]() { handleEvent(); });
        setChannel();
    });
}

void MysqlConnection::setChannel()
{
    if ((waitStatus_ & MYSQL_WAIT_READ) || (waitStatus_ & MYSQL_WAIT_EXCEPT))
    {
        if (!channelPtr_->isReading())
            channelPtr_->enableReading();
    }
    if (waitStatus_ & MYSQL_WAIT_WRITE)
    {
        if (!channelPtr_->isWriting())
            channelPtr_->enableWriting();
    }
    else
    {
        if (channelPtr_->isWriting())
            channelPtr_->disableWriting();
    }
    if (waitStatus_ & MYSQL_WAIT_TIMEOUT)
    {
        auto timeout = mysql_get_timeout_value(mysqlPtr_.get());
        auto thisPtr = shared_from_this();
        loop_->runAfter(timeout, [thisPtr]() { thisPtr->handleTimeout(); });
    }
}

void MysqlConnection::handleClosed()
{
    loop_->assertInLoopThread();
    if (status_ == ConnectStatus::Bad)
        return;
    status_ = ConnectStatus::Bad;
    channelPtr_->disableAll();
    channelPtr_->remove();
    assert(closeCallback_);
    auto thisPtr = shared_from_this();
    closeCallback_(thisPtr);
}
void MysqlConnection::disconnect()
{
    auto thisPtr = shared_from_this();
    std::promise<int> pro;
    auto f = pro.get_future();
    loop_->runInLoop([thisPtr, &pro]() {
        thisPtr->status_ = ConnectStatus::Bad;
        thisPtr->channelPtr_->disableAll();
        thisPtr->channelPtr_->remove();
        thisPtr->mysqlPtr_.reset();
        pro.set_value(1);
    });
    f.get();
}
void MysqlConnection::handleTimeout()
{
    int status = 0;
    status |= MYSQL_WAIT_TIMEOUT;
    MYSQL *ret;
    if (status_ == ConnectStatus::Connecting)
    {
        waitStatus_ = mysql_real_connect_cont(&ret, mysqlPtr_.get(), status);
        if (waitStatus_ == 0)
        {
            auto errorNo = mysql_errno(mysqlPtr_.get());
            if (!ret && errorNo)
            {
                LOG_ERROR << "Error(" << errorNo << ") \""
                          << mysql_error(mysqlPtr_.get()) << "\"";
                LOG_ERROR << "Failed to mysql_real_connect()";
                handleClosed();
                return;
            }
            // I don't think the programe can run to here.
            if (characterSet_.empty())
            {
                status_ = ConnectStatus::Ok;
                if (okCallback_)
                {
                    auto thisPtr = shared_from_this();
                    okCallback_(thisPtr);
                }
            }
            else
            {
                startSetCharacterSet();
                return;
            }
        }
        setChannel();
    }
    else if (status_ == ConnectStatus::SettingCharacterSet)
    {
        continueSetCharacterSet(status);
    }
    else if (status_ == ConnectStatus::Ok)
    {
    }
}
void MysqlConnection::handleEvent()
{
    int status = 0;
    auto revents = channelPtr_->revents();
    if (revents & POLLIN)
        status |= MYSQL_WAIT_READ;
    if (revents & POLLOUT)
        status |= MYSQL_WAIT_WRITE;
    if (revents & POLLPRI)
        status |= MYSQL_WAIT_EXCEPT;
    status = (status & waitStatus_);
    MYSQL *ret;
    if (status_ == ConnectStatus::Connecting)
    {
        waitStatus_ = mysql_real_connect_cont(&ret, mysqlPtr_.get(), status);
        if (waitStatus_ == 0)
        {
            auto errorNo = mysql_errno(mysqlPtr_.get());
            if (!ret && errorNo)
            {
                LOG_ERROR << "Error(" << errorNo << ") \""
                          << mysql_error(mysqlPtr_.get()) << "\"";
                LOG_ERROR << "Failed to mysql_real_connect()";
                handleClosed();
                return;
            }
            if (characterSet_.empty())
            {
                status_ = ConnectStatus::Ok;
                if (okCallback_)
                {
                    auto thisPtr = shared_from_this();
                    okCallback_(thisPtr);
                }
            }
            else
            {
                startSetCharacterSet();
                return;
            }
        }
        setChannel();
    }
    else if (status_ == ConnectStatus::Ok)
    {
        switch (execStatus_)
        {
            case ExecStatus::RealQuery:
            {
                int err = 0;
                waitStatus_ =
                    mysql_real_query_cont(&err, mysqlPtr_.get(), status);
                LOG_TRACE << "real_query:" << waitStatus_;
                if (waitStatus_ == 0)
                {
                    if (err)
                    {
                        execStatus_ = ExecStatus::None;
                        LOG_ERROR << "error:" << err << " status:" << status;
                        outputError();
                        return;
                    }
                    execStatus_ = ExecStatus::StoreResult;
                    MYSQL_RES *ret;
                    waitStatus_ =
                        mysql_store_result_start(&ret, mysqlPtr_.get());
                    LOG_TRACE << "store_result_start:" << waitStatus_;
                    if (waitStatus_ == 0)
                    {
                        execStatus_ = ExecStatus::None;
                        if (!ret && mysql_errno(mysqlPtr_.get()))
                        {
                            LOG_ERROR << "error in: " << sql_;
                            outputError();
                            return;
                        }
                        getResult(ret);
                    }
                }
                setChannel();
                break;
            }
            case ExecStatus::StoreResult:
            {
                MYSQL_RES *ret;
                waitStatus_ =
                    mysql_store_result_cont(&ret, mysqlPtr_.get(), status);
                LOG_TRACE << "store_result:" << waitStatus_;
                if (waitStatus_ == 0)
                {
                    if (!ret && mysql_errno(mysqlPtr_.get()))
                    {
                        execStatus_ = ExecStatus::None;
                        LOG_ERROR << "error";
                        outputError();
                        return;
                    }
                    getResult(ret);
                }
                setChannel();
                break;
            }
            case ExecStatus::None:
            {
                // Connection closed!
                if (waitStatus_ == 0)
                    handleClosed();
                break;
            }
            default:
                return;
        }
    }
    else if (status_ == ConnectStatus::SettingCharacterSet)
    {
        continueSetCharacterSet(status);
    }
}
void MysqlConnection::continueSetCharacterSet(int status)
{
    int err;
    waitStatus_ = mysql_set_character_set_cont(&err, mysqlPtr_.get(), status);
    if (waitStatus_ == 0)
    {
        if (err)
        {
            LOG_ERROR << "Error(" << err << ") \""
                      << mysql_error(mysqlPtr_.get()) << "\"";
            LOG_ERROR << "Failed to mysql_set_character_set_cont()";
            handleClosed();
            return;
        }
        status_ = ConnectStatus::Ok;
        if (okCallback_)
        {
            auto thisPtr = shared_from_this();
            okCallback_(thisPtr);
        }
    }
    setChannel();
}
void MysqlConnection::startSetCharacterSet()
{
    int err;
    waitStatus_ = mysql_set_character_set_start(&err,
                                                mysqlPtr_.get(),
                                                characterSet_.data());
    if (waitStatus_ == 0)
    {
        if (err)
        {
            LOG_ERROR << "Error(" << err << ") \""
                      << mysql_error(mysqlPtr_.get()) << "\"";
            LOG_ERROR << "Failed to mysql_set_character_set_start()";
            handleClosed();
            return;
        }
        status_ = ConnectStatus::Ok;
        if (okCallback_)
        {
            auto thisPtr = shared_from_this();
            okCallback_(thisPtr);
        }
    }
    else
    {
        status_ = ConnectStatus::SettingCharacterSet;
    }
    setChannel();
}
void MysqlConnection::execSqlInLoop(
    string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    LOG_TRACE << sql;
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(!isWorking_);
    assert(!sql.empty());

    callback_ = std::move(rcb);
    isWorking_ = true;
    exceptionCallback_ = std::move(exceptCallback);
    sql_.clear();
    if (paraNum > 0)
    {
        std::string::size_type pos = 0;
        std::string::size_type seekPos = std::string::npos;
        for (size_t i = 0; i < paraNum; ++i)
        {
            seekPos = sql.find("?", pos);
            if (seekPos == std::string::npos)
            {
                auto sub = sql.substr(pos);
                sql_.append(sub.data(), sub.length());
                pos = seekPos;
                break;
            }
            else
            {
                auto sub = sql.substr(pos, seekPos - pos);
                sql_.append(sub.data(), sub.length());
                pos = seekPos + 1;
                switch (format[i])
                {
                    case MYSQL_TYPE_TINY:
                        sql_.append(std::to_string(*((char *)parameters[i])));
                        break;
                    case MYSQL_TYPE_SHORT:
                        sql_.append(std::to_string(*((short *)parameters[i])));
                        break;
                    case MYSQL_TYPE_LONG:
                        sql_.append(
                            std::to_string(*((int32_t *)parameters[i])));
                        break;
                    case MYSQL_TYPE_LONGLONG:
                        sql_.append(
                            std::to_string(*((int64_t *)parameters[i])));
                        break;
                    case MYSQL_TYPE_NULL:
                        sql_.append("NULL");
                        break;
                    case MYSQL_TYPE_STRING:
                    {
                        sql_.append("'");
                        std::string to(length[i] * 2, '\0');
                        auto len = mysql_real_escape_string(mysqlPtr_.get(),
                                                            (char *)to.c_str(),
                                                            parameters[i],
                                                            length[i]);
                        to.resize(len);
                        sql_.append(to);
                        sql_.append("'");
                    }
                    break;
                    default:
                        break;
                }
            }
        }
        if (pos < sql.length())
        {
            auto sub = sql.substr(pos);
            sql_.append(sub.data(), sub.length());
        }
    }
    else
    {
        sql_ = std::string(sql.data(), sql.length());
    }
    LOG_TRACE << sql_;
    int err;
    // int mysql_real_query_start(int *ret, MYSQL *mysql, const char *q,
    // unsigned long length)
    waitStatus_ = mysql_real_query_start(&err,
                                         mysqlPtr_.get(),
                                         sql_.c_str(),
                                         sql_.length());
    LOG_TRACE << "real_query:" << waitStatus_;
    execStatus_ = ExecStatus::RealQuery;
    if (waitStatus_ == 0)
    {
        if (err)
        {
            LOG_ERROR << "error";
            loop_->queueInLoop(
                [thisPtr = shared_from_this()] { thisPtr->outputError(); });
            return;
        }

        MYSQL_RES *ret;
        waitStatus_ = mysql_store_result_start(&ret, mysqlPtr_.get());
        LOG_TRACE << "store_result:" << waitStatus_;
        execStatus_ = ExecStatus::StoreResult;
        if (waitStatus_ == 0)
        {
            execStatus_ = ExecStatus::None;
            if (!ret && mysql_errno(mysqlPtr_.get()))
            {
                LOG_ERROR << "error in: " << sql_;
                loop_->queueInLoop(
                    [thisPtr = shared_from_this()] { thisPtr->outputError(); });
                return;
            }
            loop_->queueInLoop([thisPtr = shared_from_this(), ret] {
                thisPtr->getResult(ret);
            });
        }
    }
    setChannel();
    return;
}

void MysqlConnection::outputError()
{
    channelPtr_->disableAll();
    auto errorNo = mysql_errno(mysqlPtr_.get());
    LOG_ERROR << "Error(" << errorNo << ") [" << mysql_sqlstate(mysqlPtr_.get())
              << "] \"" << mysql_error(mysqlPtr_.get()) << "\"";

    if (isWorking_)
    {
        try
        {
            // TODO: exception type
            throw SqlError(mysql_error(mysqlPtr_.get()), sql_);
        }
        catch (...)
        {
            exceptionCallback_(std::current_exception());
            exceptionCallback_ = nullptr;
        }

        callback_ = nullptr;
        isWorking_ = false;
        if (errorNo != CR_SERVER_GONE_ERROR && errorNo != CR_SERVER_LOST)
        {
            idleCb_();
        }
    }
    if (errorNo == CR_SERVER_GONE_ERROR || errorNo == CR_SERVER_LOST)
    {
        handleClosed();
    }
}

void MysqlConnection::getResult(MYSQL_RES *res)
{
    auto resultPtr = std::shared_ptr<MYSQL_RES>(res, [](MYSQL_RES *r) {
        mysql_free_result(r);
    });
    auto Result = makeResult(resultPtr,
                             mysql_affected_rows(mysqlPtr_.get()),
                             mysql_insert_id(mysqlPtr_.get()));
    if (isWorking_)
    {
        callback_(Result);
        callback_ = nullptr;
        exceptionCallback_ = nullptr;
        isWorking_ = false;
        idleCb_();
    }
}
