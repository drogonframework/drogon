/**
 *
 *  @file WindowsSupport.h
 *  @author An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#pragma once

#include <trantor/exports.h>
#include <stdint.h>

struct iovec
{
    void *iov_base; /* Starting address */
    int iov_len;    /* Number of bytes */
};

TRANTOR_EXPORT int readv(int fd, const struct iovec *vector, int count);
