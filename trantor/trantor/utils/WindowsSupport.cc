/**
 *
 *  WindowsSupport.cc
 *  An Tao
 *
 *  Implementation of Windows support functions.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#include <trantor/utils/WindowsSupport.h>
#include <winsock2.h>

// from polipo
int win32_read_socket(int fd, void *buf, int n)
{
    int rc = recv(fd, reinterpret_cast<char *>(buf), n, 0);
    if (rc == SOCKET_ERROR)
    {
        _set_errno(WSAGetLastError());
    }
    return rc;
}

int readv(int fd, const struct iovec *vector, int count)
{
    int ret = 0; /* Return value */
    int i;
    for (i = 0; i < count; i++)
    {
        int n = vector[i].iov_len;
        int rc = win32_read_socket(fd, vector[i].iov_base, n);
        if (rc == n)
        {
            ret += rc;
        }
        else
        {
            if (rc < 0)
            {
                ret = (ret == 0 ? rc : ret);
            }
            else
            {
                ret += rc;
            }
            break;
        }
    }
    return ret;
}
