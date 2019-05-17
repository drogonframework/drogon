#include "../lib/src/ssl_funcs/Md5.h"
#include <iostream>
int main()
{
    std::cout << Md5Encode::encode(
                     "123456789012345678901234567890123456789012345"
                     "678901234567890123456789012345678901234567890"
                     "1234567890")
              << std::endl;
}