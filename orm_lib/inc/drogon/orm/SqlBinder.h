/**
 *
 *  @file SqlBinder.h
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
#include <drogon/orm/DbTypes.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/FunctionTraits.h>
#include <drogon/orm/ResultIterator.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/RowIterator.h>
#include <drogon/utils/string_view.h>
#include <drogon/utils/optional.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/NonCopyable.h>
#include <json/json.h>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string.h>
#include <string>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#else  // some Unix-like OS
#include <arpa/inet.h>
#endif

#if defined __linux__ || defined __FreeBSD__ || defined __OpenBSD__ || \
    defined __MINGW32__ || defined __HAIKU__

#ifdef __linux__
#include <endian.h>  // __BYTE_ORDER __LITTLE_ENDIAN
#elif defined __FreeBSD__ || defined __OpenBSD__
#include <sys/endian.h>  // _BYTE_ORDER _LITTLE_ENDIAN
#define __BYTE_ORDER _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#elif defined __MINGW32__
#include <sys/param.h>  // BYTE_ORDER LITTLE_ENDIAN
#define __BYTE_ORDER BYTE_ORDER
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif

#include <algorithm>  // std::reverse()

template <typename T>
constexpr T htonT(T value) noexcept
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    char *ptr = reinterpret_cast<char *>(&value);
    std::reverse(ptr, ptr + sizeof(T));
#endif
    return value;
}
inline uint64_t htonll(uint64_t value)
{
    return htonT<uint64_t>(value);
}
inline uint64_t ntohll(uint64_t value)
{
    return htonll(value);
}
#endif

namespace drogon
{
namespace orm
{
enum class ClientType
{
    PostgreSQL = 0,
    Mysql,
    Sqlite3
};

enum Sqlite3Type
{
    Sqlite3TypeChar = 0,
    Sqlite3TypeShort,
    Sqlite3TypeInt,
    Sqlite3TypeInt64,
    Sqlite3TypeDouble,
    Sqlite3TypeText,
    Sqlite3TypeBlob,
    Sqlite3TypeNull
};

class DbClient;
using QueryCallback = std::function<void(const Result &)>;
using ExceptPtrCallback = std::function<void(const std::exception_ptr &)>;
enum class Mode
{
    NonBlocking,
    Blocking
};
namespace internal
{
template <typename T>
struct VectorTypeTraits
{
    static const bool isVector = false;
    static const bool isPtrVector = false;
    using ItemsType = T;
};
template <typename T>
struct VectorTypeTraits<std::vector<std::shared_ptr<T>>>
{
    static const bool isVector = true;
    static const bool isPtrVector = true;
    using ItemsType = T;
};
template <>
struct VectorTypeTraits<std::string>
{
    static const bool isVector = false;
    static const bool isPtrVector = false;
    using ItemsType = std::string;
};

// we only accept value type or const lreference type or rreference type as
// handle method parameters type
template <typename T>
struct CallbackArgTypeTraits
{
    static const bool isValid = true;
};
template <typename T>
struct CallbackArgTypeTraits<T *>
{
    static const bool isValid = false;
};
template <typename T>
struct CallbackArgTypeTraits<T &>
{
    static const bool isValid = false;
};
template <typename T>
struct CallbackArgTypeTraits<T &&>
{
    static const bool isValid = true;
};
template <typename T>
struct CallbackArgTypeTraits<const T &>
{
    static const bool isValid = true;
};

class CallbackHolderBase
{
  public:
    virtual ~CallbackHolderBase()
    {
    }
    virtual void execCallback(const Result &result) = 0;
};
template <typename Function>
class CallbackHolder : public CallbackHolderBase
{
  public:
    virtual void execCallback(const Result &result)
    {
        run(result);
    }

    template <typename T>
    CallbackHolder(T &&function) : function_(std::forward<T>(function))
    {
        static_assert(traits::isSqlCallback,
                      "Your sql callback function type is wrong!");
    }

  private:
    Function function_;
    using traits = FunctionTraits<Function>;
    template <std::size_t Index>
    using NthArgumentType = typename traits::template argument<Index>;
    static const size_t argumentCount = traits::arity;

    template <bool isStep = traits::isStepResultCallback>
    typename std::enable_if<isStep, void>::type run(const Result &result)
    {
        if (result.size() == 0)
        {
            run(nullptr, true);
            return;
        }
        for (auto const &row : result)
        {
            run(&row, false);
        }
        run(nullptr, true);
    }
    template <bool isStep = traits::isStepResultCallback>
    typename std::enable_if<!isStep, void>::type run(const Result &result)
    {
        static_assert(argumentCount == 0,
                      "Your sql callback function type is wrong!");
        function_(result);
    }
    template <typename... Values, std::size_t Boundary = argumentCount>
    typename std::enable_if<(sizeof...(Values) < Boundary), void>::type run(
        const Row *const row,
        bool isNull,
        Values &&...values)
    {
        // call this function recursively until parameter's count equals to the
        // count of target function parameters
        static_assert(
            CallbackArgTypeTraits<NthArgumentType<sizeof...(Values)>>::isValid,
            "your sql callback function argument type must be value "
            "type or "
            "const "
            "left-reference type");
        using ValueType =
            typename std::remove_cv<typename std::remove_reference<
                NthArgumentType<sizeof...(Values)>>::type>::type;
        ValueType value = ValueType();
        if (row && row->size() > sizeof...(Values))
        {
            // if(!VectorTypeTraits<ValueType>::isVector)
            //     value = (*row)[sizeof...(Values)].as<ValueType>();
            // else
            //     ; // value =
            //     (*row)[sizeof...(Values)].asArray<VectorTypeTraits<ValueType>::ItemsType>();
            value =
                makeValue<ValueType>((*row)[(Row::SizeType)sizeof...(Values)]);
        }

        run(row, isNull, std::forward<Values>(values)..., std::move(value));
    }
    template <typename... Values, std::size_t Boundary = argumentCount>
    typename std::enable_if<(sizeof...(Values) == Boundary), void>::type run(
        const Row *const,
        bool isNull,
        Values &&...values)
    {
        function_(isNull, std::move(values)...);
    }
    template <typename ValueType>
    typename std::enable_if<VectorTypeTraits<ValueType>::isVector,
                            ValueType>::type
    makeValue(const Field &field)
    {
        return field.asArray<typename VectorTypeTraits<ValueType>::ItemsType>();
    }
    template <typename ValueType>
    typename std::enable_if<!VectorTypeTraits<ValueType>::isVector,
                            ValueType>::type
    makeValue(const Field &field)
    {
        return field.as<ValueType>();
    }
};
class DROGON_EXPORT SqlBinder : public trantor::NonCopyable
{
    using self = SqlBinder;

  public:
    friend class Dbclient;

    SqlBinder(const std::string &sql, DbClient &client, ClientType type)
        : sqlPtr_(std::make_shared<std::string>(sql)),
          sqlViewPtr_(sqlPtr_->data()),
          sqlViewLength_(sqlPtr_->length()),
          client_(client),
          type_(type)
    {
    }
    SqlBinder(std::string &&sql, DbClient &client, ClientType type)
        : sqlPtr_(std::make_shared<std::string>(std::move(sql))),
          sqlViewPtr_(sqlPtr_->data()),
          sqlViewLength_(sqlPtr_->length()),
          client_(client),
          type_(type)
    {
    }
    SqlBinder(const char *sql,
              size_t sqlLength,
              DbClient &client,
              ClientType type)
        : sqlViewPtr_(sql),
          sqlViewLength_(sqlLength),
          client_(client),
          type_(type)
    {
    }
    SqlBinder(SqlBinder &&that)
        : sqlPtr_(std::move(that.sqlPtr_)),
          sqlViewPtr_(that.sqlViewPtr_),
          sqlViewLength_(that.sqlViewLength_),
          client_(that.client_),
          parametersNumber_(that.parametersNumber_),
          parameters_(std::move(that.parameters_)),
          lengths_(std::move(that.lengths_)),
          formats_(std::move(that.formats_)),
          objs_(std::move(that.objs_)),
          mode_(that.mode_),
          callbackHolder_(std::move(that.callbackHolder_)),
          exceptionCallback_(std::move(that.exceptionCallback_)),
          exceptionPtrCallback_(std::move(that.exceptionPtrCallback_)),
          execed_(that.execed_),
          destructed_(that.destructed_),
          isExceptionPtr_(that.isExceptionPtr_),
          type_(that.type_)
    {
        // set the execed_ to true to avoid the same sql being executed twice.
        that.execed_ = true;
    }
    SqlBinder &operator=(SqlBinder &&that) = delete;
    ~SqlBinder();
    template <typename CallbackType,
              typename traits =
                  FunctionTraits<typename std::decay<CallbackType>::type>>
    typename std::enable_if<traits::isExceptCallback && traits::isPtr,
                            self>::type &
    operator>>(CallbackType &&callback)
    {
        // LOG_DEBUG << "ptr callback";
        isExceptionPtr_ = true;
        exceptionPtrCallback_ = std::forward<CallbackType>(callback);
        return *this;
    }

    template <typename CallbackType,
              typename traits =
                  FunctionTraits<typename std::decay<CallbackType>::type>>
    typename std::enable_if<traits::isExceptCallback && !traits::isPtr,
                            self>::type &
    operator>>(CallbackType &&callback)
    {
        isExceptionPtr_ = false;
        exceptionCallback_ = std::forward<CallbackType>(callback);
        return *this;
    }

    template <typename CallbackType,
              typename traits =
                  FunctionTraits<typename std::decay<CallbackType>::type>>
    typename std::enable_if<traits::isSqlCallback, self>::type &operator>>(
        CallbackType &&callback)
    {
        callbackHolder_ = std::shared_ptr<CallbackHolderBase>(
            new CallbackHolder<typename std::decay<CallbackType>::type>(
                std::forward<CallbackType>(callback)));
        return *this;
    }

    template <typename T>
    typename std::enable_if<
        !std::is_same<typename std::remove_cv<
                          typename std::remove_reference<T>::type>::type,
                      trantor::Date>::value,
        self &>::type
    operator<<(T &&parameter)
    {
        ++parametersNumber_;
        using ParaType = typename std::remove_cv<
            typename std::remove_reference<T>::type>::type;
        std::shared_ptr<void> obj = std::make_shared<ParaType>(parameter);
        if (type_ == ClientType::PostgreSQL)
        {
#if __cplusplus >= 201703L || (defined _MSC_VER && _MSC_VER > 1900)
            const size_t size = sizeof(T);
            if constexpr (size == 2)
            {
                *std::static_pointer_cast<uint16_t>(obj) = htons(parameter);
            }
            else if constexpr (size == 4)
            {
                *std::static_pointer_cast<uint32_t>(obj) = htonl(parameter);
            }
            else if constexpr (size == 8)
            {
                *std::static_pointer_cast<uint64_t>(obj) = htonll(parameter);
            }
#else
            switch (sizeof(T))
            {
                case 2:
                    *std::static_pointer_cast<uint16_t>(obj) = htons(parameter);
                    break;
                case 4:
                    *std::static_pointer_cast<uint32_t>(obj) = htonl(parameter);
                    break;
                case 8:
                    *std::static_pointer_cast<uint64_t>(obj) =
                        htonll(parameter);
                    break;
                case 1:
                default:

                    break;
            }
#endif
            objs_.push_back(obj);
            parameters_.push_back((char *)obj.get());
            lengths_.push_back(sizeof(T));
            formats_.push_back(1);
        }
        else if (type_ == ClientType::Mysql)
        {
            objs_.push_back(obj);
            parameters_.push_back((char *)obj.get());
            lengths_.push_back(0);
            formats_.push_back(getMysqlTypeBySize(sizeof(T)));
        }
        else if (type_ == ClientType::Sqlite3)
        {
            objs_.push_back(obj);
            parameters_.push_back((char *)obj.get());
            lengths_.push_back(0);
            switch (sizeof(T))
            {
                case 1:
                    formats_.push_back(Sqlite3TypeChar);
                    break;
                case 2:
                    formats_.push_back(Sqlite3TypeShort);
                    break;
                case 4:
                    formats_.push_back(Sqlite3TypeInt);
                    break;
                case 8:
                    formats_.push_back(Sqlite3TypeInt64);
                default:
                    break;
            }
        }
        // LOG_TRACE << "Bind parameter:" << parameter;
        return *this;
    }
    // template <>
    self &operator<<(const char str[])
    {
        return operator<<(std::string(str));
    }
    self &operator<<(char str[])
    {
        return operator<<(std::string(str));
    }
    self &operator<<(const std::string &str);
    self &operator<<(std::string &str)
    {
        return operator<<((const std::string &)str);
    }
    self &operator<<(std::string &&str);
    self &operator<<(trantor::Date &&date)
    {
        return operator<<(date.toDbStringLocal());
    }
    self &operator<<(const trantor::Date &date)
    {
        return operator<<(date.toDbStringLocal());
    }
    self &operator<<(const std::vector<char> &v);
    self &operator<<(std::vector<char> &v)
    {
        return operator<<((const std::vector<char> &)v);
    }
    self &operator<<(std::vector<char> &&v);
    self &operator<<(float f)
    {
        if (type_ == ClientType::Sqlite3)
        {
            return operator<<((double)f);
        }
        return operator<<(std::to_string(f));
    }
    self &operator<<(double f);
    self &operator<<(std::nullptr_t nullp);
    self &operator<<(DefaultValue dv);
    self &operator<<(const Mode &mode)
    {
        mode_ = mode;
        return *this;
    }
    self &operator<<(Mode &mode)
    {
        mode_ = mode;
        return *this;
    }
    self &operator<<(Mode &&mode)
    {
        mode_ = mode;
        return *this;
    }
    template <typename T>
    self &operator<<(const optional<T> &parameter)
    {
        if (parameter)
        {
            return *this << parameter.value();
        }
        return *this << nullptr;
    }
    template <typename T>
    self &operator<<(optional<T> &parameter)
    {
        if (parameter)
        {
            return *this << parameter.value();
        }
        return *this << nullptr;
    }
    template <typename T>
    self &operator<<(optional<T> &&parameter)
    {
        if (parameter)
        {
            return *this << std::move(parameter.value());
        }
        return *this << nullptr;
    }
    self &operator<<(const Json::Value &j) noexcept(true)
    {
        switch (j.type())
        {
            case Json::nullValue:
                return *this << nullptr;
                break;
            case Json::intValue:
                return *this << j.asInt64();
                break;
            case Json::uintValue:
                return *this << j.asUInt64();
                break;
            case Json::realValue:
                return *this << j.asDouble();
                break;
            case Json::stringValue:
                return *this << j.asString();
                break;
            case Json::booleanValue:
                return *this << j.asBool();
                break;
            case Json::arrayValue:
            case Json::objectValue:
            default:
                LOG_ERROR << "Bad Json type";
                return *this << nullptr;
                break;
        }
    }
    self &operator<<(Json::Value &j) noexcept(true)
    {
        return *this << static_cast<const Json::Value &>(j);
    }
    self &operator<<(Json::Value &&j) noexcept(true)
    {
        return *this << static_cast<const Json::Value &>(j);
    }
    void exec() noexcept(false);

  private:
    int getMysqlTypeBySize(size_t size);
    std::shared_ptr<std::string> sqlPtr_;
    const char *sqlViewPtr_;
    size_t sqlViewLength_;
    DbClient &client_;
    size_t parametersNumber_{0};
    std::vector<const char *> parameters_;
    std::vector<int> lengths_;
    std::vector<int> formats_;
    std::vector<std::shared_ptr<void>> objs_;
    Mode mode_{Mode::NonBlocking};
    std::shared_ptr<CallbackHolderBase> callbackHolder_;
    DrogonDbExceptionCallback exceptionCallback_;
    ExceptPtrCallback exceptionPtrCallback_;
    bool execed_{false};
    bool destructed_{false};
    bool isExceptionPtr_{false};
    ClientType type_;
};

}  // namespace internal
}  // namespace orm
}  // namespace drogon
