#include <drogon/utils/Utilities.h>
#include <iostream>

int main()
{
    std::string input = "k1=1&k2=安";
    auto output = drogon::urlEncode(input);
    std::cout << output << std::endl;
    auto output2 = drogon::urlDecode(output);
    std::cout << output2 << std::endl;
    std::cout << drogon::urlDecode("k2%3D%E5%AE%89&k1%3D1");
    return 0;
}