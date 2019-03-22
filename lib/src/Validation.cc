#include <drogon/Validation.h>

using namespace drogon;

Validation::Validation() noexcept {
    this->errorMessages = "";
}

Validation::~Validation() noexcept {  }

bool Validation::validateAll() noexcept { 
    return this->errorMessages == "";
}

string Validation::getMessages() noexcept { 
    return this->errorMessages.substr(0, this->errorMessages.size() - 1);
}

void Validation::notNull(const HttpRequestPtr& request, const string& field, const string& message) noexcept {
    auto& value = request->getParameter(field, "");
    if (value == "") {
        this->errorMessages += message + ",";
    }
}

void Validation::isNumeric(const HttpRequestPtr& request, const string& field, const string& message) noexcept {
    auto& value = request->getParameter(field, "");
    if (!isInteger(value)) {
        this->errorMessages += message + ",";
    }
}

void Validation::stringLength(const HttpRequestPtr& request, const string& field, const int min, const int max, const string& message) noexcept {
    auto& value = request->getParameter(field, "");
    if (!isInteger(value)) { //如果不是数字
        this->errorMessages += message + ",";
    }

    int length = value.length();
    if (length < min || length > max) { 
        this->errorMessages += message + ",";
    }
}

