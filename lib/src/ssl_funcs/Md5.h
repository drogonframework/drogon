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

#ifndef __MD5_ENCODE_H__
#define __MD5_ENCODE_H__

// std
#include <string>

// define
#define UInt32 unsigned int
#define BIT_OF_BYTE 8
#define BIT_OF_GROUP 512
#define SRC_DATA_LEN 64

// 四个非线性函数宏定义
#define DEF_F(X, Y, Z) ((((X) & (Y)) | ((~X) & (Z))))
#define DEF_G(X, Y, Z) (((X) & (Z)) | ((Y) & (~Z)))
#define DEF_H(X, Y, Z) ((X) ^ (Y) ^ (Z))
#define DEF_I(X, Y, Z) ((Y) ^ ((X) | (~Z)))

// 求链接数函数宏定义
#define FF(a, b, c, d, Mj, s, ti) (a = b + CycleMoveLeft((a + DEF_F(b, c, d) + Mj + ti), s));
#define GG(a, b, c, d, Mj, s, ti) (a = b + CycleMoveLeft((a + DEF_G(b, c, d) + Mj + ti), s));
#define HH(a, b, c, d, Mj, s, ti) (a = b + CycleMoveLeft((a + DEF_H(b, c, d) + Mj + ti), s));
#define II(a, b, c, d, Mj, s, ti) (a = b + CycleMoveLeft((a + DEF_I(b, c, d) + Mj + ti), s));

class Md5Encode
{
  public:
    // 4轮循环算法
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
    Md5Encode()
    {
    }
    std::string Encode(std::string src_info);

  protected:
    UInt32 CycleMoveLeft(UInt32 src_num, int bit_num_to_move);
    UInt32 FillData(const char *in_data_ptr, int data_byte_len, char **out_data_ptr);
    void RoundF(char *data_512_ptr, ParamDynamic &param);
    void RoundG(char *data_512_ptr, ParamDynamic &param);
    void RoundH(char *data_512_ptr, ParamDynamic &param);
    void RoundI(char *data_512_ptr, ParamDynamic &param);
    void RotationCalculate(char *data_512_ptr, ParamDynamic &param);
    std::string GetHexStr(unsigned int num_str);

  private:
    // 幻数定义
    static const int kA;
    static const int kB;
    static const int kC;
    static const int kD;
    static const unsigned long long k_ti_num_integer;
};

#endif
