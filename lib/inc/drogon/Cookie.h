/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */
#pragma once

#include <trantor/utils/Date.h>
#include <string>

namespace drogon{
    class Cookie{
    public:
        Cookie(const std::string &key,const std::string &value):
                _key(key),
                _value(value)
        {}
        ~Cookie(){}
        void setExpiresDate(const trantor::Date &date){_expiresDate=date;}
        void setHttpOnly(bool only){_httpOnly=only;}
        void setEnsure(bool ensure){_ensure=ensure;}
        void setDomain(const std::string &domain){_domain=domain;}
        void setPath(const std::string &path){_path=path;}
        const std::string cookieString() const;
        const std::string &key() const {return _key;}
        const std::string &value() const {return _value;}
    private:
        trantor::Date _expiresDate=trantor::Date();
        bool _httpOnly=true;
        bool _ensure=false;
        std::string _domain;
        std::string _path;
        std::string _key;
        std::string _value;
    };
}