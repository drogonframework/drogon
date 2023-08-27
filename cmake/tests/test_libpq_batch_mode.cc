/**
 *
 *  Copyright 2023, Drogon Framework Team. All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 */

#include <libpq-fe.h>

int main() {
    PQenterPipelineMode(NULL);
    PQexitPipelineMode(NULL);
    PQpipelineSync(NULL);
    PQpipelineStatus(NULL);
}
