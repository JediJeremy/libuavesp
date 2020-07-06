#include "primitive.h"



/*! \brief Return the index of the most significent bit set. 8==msb, 1==lsb, 0==none
    \param f the byte to search for bits
*/
uint8_t first_uint8_onebit(uint8_t v) {
    if(v==0) return 0;
    if(v & 0b11110000) {
        if(v & 0b11000000) {
            return (v & 0b10000000) ? 8 : 7;
        } else {
            return (v & 0b00100000) ? 6 : 5;
        }
    } else {
        if(v & 0b00001100) {
            return (v & 0b00001000) ? 4 : 3;
        } else {
            return (v & 0b00000010) ? 2 : 1;
        }
    }
}

/// 
/// http://www.fox-toolkit.org/ftp/fasthalffloatconversion.pdf

/*! \brief Converts a Float16 value to standard c float \a fd.
    \param f The float16 value in a 16 bit integer wrapper format.
*/
float fp16_to_float(uint16_t f) {
    float r = 0;
    uint32_t* bv = (uint32_t*)&r;
    // fp16 properties
    uint32_t fp16_sign = (f >> 15) & 1;
    uint32_t fp16_exp = (f >> 10) & 0x1F;
    uint32_t fp16_mantissa = f & 0x03FF;
    // fp32 properties
    uint32_t fp32_sign = fp16_sign << 31;
    // is it one of the special conditions?
    if(fp16_exp==0x1F) {
        // sign bit plus infinity exponent plus NaN junk
        *bv = fp32_sign | 0x7F80000 | (fp16_mantissa<<13);
    } else if(fp16_exp==0) {
        // subnormal (small) number   = (-1)s ∙ 2-14 ∙ 0.mmmmmmmmmm
        if(fp16_mantissa==0) {
            // signed zero. easy.
            *bv = fp32_sign;
        } else {
            // where is the first binary 1 digit in the 10-bit mantissa?
            int fp16_digit;
            if(fp16_mantissa & 0x0300) {
                // in the upper two bits
                fp16_digit = (fp16_mantissa & 0x0200) ? 10 : 9;
            } else {
                // in the lower eight bits
                fp16_digit = first_uint8_onebit(fp16_mantissa);
            }
            // how far extra do we pad over to get the leading 1 to the other size of the dp?
            int fp16_pad = 10 - fp16_digit+1;
            // what exponent will the padded value have
            uint32_t fp32_exp = 127-15-fp16_pad;
            // just zero-pad the lsb of the number
            *bv = fp32_sign | (fp32_exp << 23) | ( (fp16_mantissa<<(13+fp16_pad)) & 0x007FFFFF);
        }
    } else {
        // normalized number  = (-1)s ∙ 2(eeeee-15) ∙ 1.mmmmmmmmmm
        uint32_t fp32_exp = fp16_exp+(127-15);
        *bv = fp32_sign | (fp32_exp << 23) | (fp16_mantissa<<13);
    }
    return r;
}

/*! \brief Converts a standard c float \a f to a Float16 value encoded into a uint16 binary.
    \param f The float value
*/
uint16_t float_to_fp16(float f) {
    uint32_t bv = *(uint32_t*)&f;
    // fp16 properties
    uint32_t fp32_sign = (bv >> 31) & 1;
    uint32_t fp32_exp = (bv >> 22) & 0xFF;
    uint32_t fp32_mantissa = bv & 0x007FFFFF;
    // get the unbased exponent 
    int f_e = fp32_exp - 127;
    // start building the fp16 properties
    uint32_t fp16_sign = fp32_sign << 15;
    uint32_t fp16_exp = f_e + 15;
    uint32_t fp16_mantissa = fp32_mantissa >> 13;
    uint16_t r;
    // is it one of the special conditions?
    if(fp32_exp==0xFF) {
        // infinity or NaNs
        r = fp16_sign | 0x7F80000 | fp16_mantissa;
    } else if(f_e<-24) {
        // too small number, maps to zero
        r = fp16_sign | 0;
    } else if(f_e<-14) {
        // subnormal number.
        r = fp16_sign | (fp16_mantissa>>(-f_e));
    } else if(f_e<=15) {
        // normalized number  = (-1)s ∙ 2(eeeee-15) ∙ 1.mmmmmmmmmm
        r = fp16_sign | (fp16_exp << 23) | fp16_mantissa;
    } else {
        // large numbers map to infinity
        r = fp16_sign | 0x7F80000;
    }
    return r;
}


