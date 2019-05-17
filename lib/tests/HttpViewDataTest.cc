#include <drogon/HttpViewData.h>
#include <iostream>
int main()
{
    drogon::HttpViewData data;
    std::cout << (data.insert("1", 1), data.get<int>("1")) << std::endl;
    std::cout << (data.insertAsString("2", 2.0), data.get<std::string>("2")) << std::endl;
    std::cout << (data.insertFormattedString("3", "third value is %d", 3), data.get<std::string>("3")) << std::endl;
    std::cout << (data.insertAsString("4", "4"), data.get<std::string>("4")) << std::endl;
}
