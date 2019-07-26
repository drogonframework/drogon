#include <drogon/utils/string_view.h>
#include <drogon/utils/Utilities.h>
#include <iostream>

int main()
{
    std::string input = "k1=1&k2=安";
    auto output = drogon::utils::urlEncode(input);
    std::cout << output << std::endl;
    auto output2 = drogon::utils::urlDecode(output);
    std::cout << output2 << std::endl;
    std::cout << drogon::utils::urlEncode("k2=安&k1=1") << std::endl;
    std::cout << drogon::utils::urlDecode(
                     drogon::string_view("k2%3D%E5%AE%89&k1%3D1"))
              << std::endl;
    return 0;
}