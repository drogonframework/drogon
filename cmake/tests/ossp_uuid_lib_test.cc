/**
 *
 *  Copyright 2023, Drogon Framework Team. All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 */

#include <uuid.h>

int main() {
    uuid_t *uuid;
    uuid_create(&uuid);
    uuid_make(uuid, UUID_MAKE_V1);
    return 0;
}
