#ifndef FUNCS_H_
#define FUNCS_H_

#include <uuid/uuid.h>
#include <string>
#include <vector>
#include <pqxx/pqxx>

#define USING_PQXX_TO_STRING

using namespace pqxx;

bool isInteger(const std::string &str);
std::string genRandomString(int length);
std::string StringToHex(unsigned char* ptr, long long length);
std::vector<std::string> splitString(const std::string &str,const std::string &separator);
std::string getuuid();
std::string stringPreProcess(const std::string& str);
std::string encryptPassword(const std::string& plainPasswd,const std::string& salt);
std::string encryptHashPassword(const std::string& hashPasswd,const std::string& salt);
template <typename T>
std::string toString(const T &obj)
{
    #ifdef USING_PQXX_TO_STRING
    return pqxx::to_string(obj);
    #else
    char str[32];
    if(typeid(obj).name() == typeid(long).name())
    {
        sprintf(str, "%ld", obj);
        return std::string(str);
    }
    else if(typeid(obj).name() == typeid(int).name())
    {
        sprintf(str, "%d", obj);
        return std::string(str);
    }
	else if(typeid(obj).name() == typeid(std::string).name())
    {
        return obj;
    }
	else if(typeid(obj).name() == typeid(bool).name())
    {
		return obj ? "true" : "false";
    }
    else
    {
        LOG_ERROR<<"no way to transfer to string"
    }
    #endif
}

template <typename T>
void fromString(const std::string& str, T &obj)
{
    #ifdef USING_PQXX_TO_STRING
    pqxx::from_string(str, obj);
    #endif
}

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::string base64_decode(std::string const& encoded_string);
std::string UrlDecode(const std::string& szToDecode);
#endif

