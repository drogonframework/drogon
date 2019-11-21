/**
 *
 *  Sha1.cc
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

#include "Sha1.h"
#include <string.h>

static inline unsigned int fromBigEndian(unsigned int v)
{
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) |
           ((v & 0xff000000) >> 24);
}

static void writeBigEndian64(unsigned char *p, unsigned int v)
{
    memset(p, 0, 8);
    memcpy(p, &v, 4);
    int i = 0;
    for (i = 0; i < 4; ++i)
    {
        unsigned char t = p[i];
        p[i] = p[7 - i];
        p[7 - i] = t;
    }
}

static inline unsigned int leftRoll(unsigned int v, int n)
{
    return (v << n) | (v >> (32 - n));
}

unsigned char *SHA1(const unsigned char *dataIn,
                    size_t dataLen,
                    unsigned char *dataOut)
{
    unsigned char *pbytes = (unsigned char *)dataIn;
    unsigned int nbyte = dataLen;

    static unsigned int words[80];
    unsigned int H[5] = {
        0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    unsigned int f, k, temp, bitlen[2], word;
    unsigned int i, j, index, p1, p2, maxlen;
    unsigned char spec[4] = {0};
    i = nbyte % 4;

    p1 = nbyte - i;
    spec[i] = 1 << 7;
    while (i--)
    {
        spec[i] = pbytes[p1 + i];
    }

    maxlen = (nbyte + 1) % 64;
    if (maxlen <= 56)
    {
        maxlen = (nbyte + 1) - maxlen + 64;
    }
    else
    {
        maxlen = (nbyte + 1) - maxlen + 128;
    }
    p2 = maxlen - 8;
    writeBigEndian64((unsigned char *)bitlen, nbyte * 8);

    for (j = 0; j < maxlen; j += 64)
    {
        unsigned int a, b, c, d, e;
        a = H[0];
        b = H[1];
        c = H[2];
        d = H[3];
        e = H[4];
        for (i = 0; i < 80; ++i)
        {
            if (i < 16)
            {
                index = j + (i << 2);
                if (index < p1)
                {
                    word = *((unsigned int *)(pbytes + index));
                }
                else if (index == p1)
                {
                    word = *(unsigned int *)spec;
                }
                else if (index < p2)
                {
                    word = 0;
                }
                else
                {
                    word = (index < maxlen - 4) ? bitlen[0] : bitlen[1];
                }
                words[i] = fromBigEndian(word);
            }
            else
            {
                words[i] = leftRoll(words[i - 3] ^ words[i - 8] ^
                                        words[i - 14] ^ words[i - 16],
                                    1);
            }
            if (i < 20)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            temp = leftRoll(a, 5) + f + e + k + words[i];
            e = d;
            d = c;
            c = leftRoll(b, 30);
            b = a;
            a = temp;
        }
        H[0] += a;
        H[1] += b;
        H[2] += c;
        H[3] += d;
        H[4] += e;
    }
    int ct = 0;
    for (i = 0; i < 5; ++i)
    {
        unsigned char buf[4] = {0};
        memcpy(buf, &(H[i]), 4);
        for (int r = 3; r >= 0; r--)
        {
            dataOut[ct] = buf[r];
            ++ct;
        }
    }

    return dataOut;
}
