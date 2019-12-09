#include <uuid.h>
int main()
{
    uuid_t uu;
    uuid_generate(uu);
    return 0;
}