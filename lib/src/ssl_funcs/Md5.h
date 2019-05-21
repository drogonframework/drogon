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

#define UInt32 unsigned int
#define BIT_OF_BYTE 8
#define BIT_OF_GROUP 512
#define SRC_DATA_LEN 64

#define DEF_F(X, Y, Z) ((((X) & (Y)) | ((~X) & (Z))))
#define DEF_G(X, Y, Z) (((X) & (Z)) | ((Y) & (~Z)))
#define DEF_H(X, Y, Z) ((X) ^ (Y) ^ (Z))
#define DEF_I(X, Y, Z) ((Y) ^ ((X) | (~Z)))

#define FF(a, b, c, d, Mj, s, ti) \
    (a = b + CycleMoveLeft((a + DEF_F(b, c, d) + Mj + ti), s));
#define GG(a, b, c, d, Mj, s, ti) \
    (a = b + CycleMoveLeft((a + DEF_G(b, c, d) + Mj + ti), s));
#define HH(a, b, c, d, Mj, s, ti) \
    (a = b + CycleMoveLeft((a + DEF_H(b, c, d) + Mj + ti), s));
#define II(a, b, c, d, Mj, s, ti) \
    (a = b + CycleMoveLeft((a + DEF_I(b, c, d) + Mj + ti), s));

class Md5Encode
{
  public:
    struct ParamDynamic
    {
        UInt32 ua_;
        UInt32 ub_;
        UInt32 uc_;
        UInt32 ud_;
        UInt32 va_last_;
        UInt32 vb_last_;
        UInt32 vc_last_;
        UInt32 vd_last_;
    };

  public:
    static std::string encode(const std::string &src_info);

  protected:
    static UInt32 CycleMoveLeft(UInt32 src_num, int bit_num_to_move);
    static void RoundF(char *data_512_ptr, ParamDynamic &param);
    static void RoundG(char *data_512_ptr, ParamDynamic &param);
    static void RoundH(char *data_512_ptr, ParamDynamic &param);
    static void RoundI(char *data_512_ptr, ParamDynamic &param);
    static void RotationCalculate(char *data_512_ptr, ParamDynamic &param);
    static std::string GetHexStr(unsigned int num_str);
    static UInt32 FillData(const char *in_data_ptr,
                           int data_byte_len,
                           char **out_data_ptr);

  private:
    static const int kA;
    static const int kB;
    static const int kC;
    static const int kD;
    static const unsigned long long k_ti_num_integer;
};
