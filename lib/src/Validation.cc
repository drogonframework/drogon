#include <drogon/Validation.h>

using namespace drogon;

Validation::Validation() noexcept 
{ 
}

Validation::~Validation() noexcept 
{  
}

bool Validation::validateAll() noexcept 
{ 
    return this->_errorMessages.empty();
}

std::string Validation::getMessages() noexcept 
{ 
    if (this->_errorMessages.empty()) { 
        return "";
    }
    return this->_errorMessages.substr(0, this->_errorMessages.size() - 1);
}

void Validation::notNull(const HttpRequestPtr& request, const std::string& field, 
        const std::string& message) noexcept 
{
    auto& value = request->getParameter(field, "");
    if (value == "") {
        this->_errorMessages += message + ",";
    }
}

void Validation::isNumeric(const HttpRequestPtr& request, const std::string& field, 
        const std::string& message) noexcept 
{
    auto& value = request->getParameter(field, "");
    if (!isInteger(value)) {
        this->_errorMessages += message + ",";
    }
}

void Validation::stringLength(const HttpRequestPtr& request, 
        const std::string& field, 
        const int min, const int max, const std::string& message) noexcept 
{
    auto& value = request->getParameter(field, "");
    if (!isInteger(value)) { //如果不是数字
        this->_errorMessages += message + ",";
    }

    int length = value.length();
    if (length < min || length > max) { 
        this->_errorMessages += message + ",";
    }
}

