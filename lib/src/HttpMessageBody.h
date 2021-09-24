/**
 *
 *  HttpMessageBody.h
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
#include <drogon/utils/string_view.h>
#include <memory>
#include <string>

namespace drogon
{
class HttpMessageBody
{
  public:
    enum class BodyType
    {
        kNone = 0,
        kString,
        kStringView
    };
    BodyType bodyType()
    {
        return type_;
    }
    virtual const char *data() const
    {
        return nullptr;
    }
    virtual char *data()
    {
        return nullptr;
    }
    virtual size_t length() const
    {
        return 0;
    }
    virtual const std::string &getString() const = 0;
    virtual std::string &getString() = 0;
    virtual void append(const char * /*buf*/, size_t /*len*/)
    {
    }
    virtual ~HttpMessageBody()
    {
    }

  protected:
    BodyType type_{BodyType::kNone};
};
class HttpMessageStringBody : public HttpMessageBody
{
  public:
    HttpMessageStringBody()
    {
        type_ = BodyType::kString;
    }
    HttpMessageStringBody(const std::string &body) : body_(body)
    {
        type_ = BodyType::kString;
    }
    HttpMessageStringBody(std::string &&body) : body_(std::move(body))
    {
        type_ = BodyType::kString;
    }
    virtual const char *data() const override
    {
        return body_.data();
    }
    virtual char *data() override
    {
        return const_cast<char *>(body_.data());
    }
    virtual size_t length() const override
    {
        return body_.length();
    }
    virtual const std::string &getString() const override
    {
        return body_;
    }
    virtual std::string &getString() override
    {
        return body_;
    }
    virtual void append(const char *buf, size_t len) override
    {
        body_.append(buf, len);
    }

  private:
    std::string body_;
};

class HttpMessageStringViewBody : public HttpMessageBody
{
  public:
    HttpMessageStringViewBody(const char *buf, size_t len) : body_(buf, len)
    {
        type_ = BodyType::kStringView;
    }
    virtual const char *data() const override
    {
        return body_.data();
    }
    virtual char *data() override
    {
        return const_cast<char *>(body_.data());
    }
    virtual size_t length() const override
    {
        return body_.length();
    }
    virtual const std::string &getString() const override
    {
        if (!bodyString_)
        {
            if (!body_.empty())
            {
                bodyString_ =
                    std::make_unique<std::string>(body_.data(), body_.length());
            }
            else
            {
                bodyString_ = std::make_unique<std::string>();
            }
        }
        return *bodyString_;
    }
    virtual std::string &getString() override
    {
        if (!bodyString_)
        {
            if (!body_.empty())
            {
                bodyString_ =
                    std::make_unique<std::string>(body_.data(), body_.length());
            }
            else
            {
                bodyString_ = std::make_unique<std::string>();
            }
        }
        return *bodyString_;
    }

  private:
    string_view body_;
    mutable std::unique_ptr<std::string> bodyString_;
};

}  // namespace drogon
