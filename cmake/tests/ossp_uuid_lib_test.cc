#include <uuid.h>
int main()
{
    uuid_t *uuid;
    uuid_create(&uuid);
    uuid_make(uuid, UUID_MAKE_V1);
    return 0;
}