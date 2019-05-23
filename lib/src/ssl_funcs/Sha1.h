/**
 *
 *  Sha1.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <iostream>

#define SHA_DIGEST_LENGTH 20

unsigned char *SHA1(const unsigned char *dataIn,
                    size_t dataLen,
                    unsigned char *dataOut);
