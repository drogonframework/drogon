/*
*******************************************************
* brief: md5 encryption
* author: Monkey.Knight
*******************************************************
*/

/**
 *
 *  Md5.h
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

#include <string>
#include <cstdint>

#define BIT_OF_BYTE 8
#define BIT_OF_GROUP 512
#define SRC_DATA_LEN 64

#define DEF_F(X, Y, Z) ((((X) & (Y)) | ((~X) & (Z))))
#define DEF_G(X, Y, Z) (((X) & (Z)) | ((Y) & (~Z)))
#define DEF_H(X, Y, Z) ((X) ^ (Y) ^ (Z))
#define DEF_I(X, Y, Z) ((Y) ^ ((X) | (~Z)))

#define FF(a, b, c, d, Mj, s, ti) \
    (a = b + cycleMoveLeft((a + DEF_F(b, c, d) + Mj + ti), s));
#define GG(a, b, c, d, Mj, s, ti) \
    (a = b + cycleMoveLeft((a + DEF_G(b, c, d) + Mj + ti), s));
#define HH(a, b, c, d, Mj, s, ti) \
    (a = b + cycleMoveLeft((a + DEF_H(b, c, d) + Mj + ti), s));
#define II(a, b, c, d, Mj, s, ti) \
    (a = b + cycleMoveLeft((a + DEF_I(b, c, d) + Mj + ti), s));

class Md5Encode
{
  public:
    struct ParamDynamic
    {
        uint32_t ua_;
        uint32_t ub_;
        uint32_t uc_;
        uint32_t ud_;
        uint32_t va_last_;
        uint32_t vb_last_;
        uint32_t vc_last_;
        uint32_t vd_last_;
    };

  public:
    static std::string encode(const char *data, const size_t dataLen);

  protected:
    static uint32_t cycleMoveLeft(uint32_t srcNum, int bitNumToMove);
    static void roundF(char *data512Ptr, ParamDynamic &param);
    static void roundG(char *data512Ptr, ParamDynamic &param);
    static void roundH(char *data512Ptr, ParamDynamic &param);
    static void roundI(char *data512Ptr, ParamDynamic &param);
    static void rotationCalculate(char *data512Ptr, ParamDynamic &param);
    static std::string getHexStr(uint32_t numStr);
    static uint32_t fillData(const char *inDataPtr,
                             int dataByteLen,
                             char **outDataPtr);

  private:
    static const uint32_t kA;
    static const uint32_t kB;
    static const uint32_t kC;
    static const uint32_t kD;
    static const uint64_t tiNumInteger;
};
