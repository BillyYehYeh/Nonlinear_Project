// Simple conversion between float and fp16 (IEEE 754 half) for test reference
// 實作 round-to-nearest-even，處理 normal / subnormal / overflow / underflow / Inf / NaN
static uint16_t float_to_fp16(float f){
    uint32_t w; std::memcpy(&w,&f,4);
    uint32_t sign = (w >> 31) & 0x1;
    uint32_t exp  = (w >> 23) & 0xFF;     // float exponent
    uint32_t mant = w & 0x7FFFFF;         // float mantissa (23 bits)

    // 特殊值: Inf / NaN
    if(exp == 0xFF){
        if(mant == 0){ // Inf
            return (sign << 15) | 0x7C00;
        }
        // NaN: 轉成 quiet, 保留高 payload bits (截斷)
        uint16_t payload = (mant >> 13) & 0x01FF; // 9 bits payload
        return (sign << 15) | 0x7C00 | 0x0200 | payload; // 1xxx xxxx xxxx: quiet NaN
    }

    // 處理非規格化 (float) -> 轉成規格化方便後續 (將 exp 調到 1 並左移 mant)
    if(exp == 0){
        if(mant == 0){ // ±0
            return (sign << 15);
        }
        // 將 subnormal float 正規化: 找到 leading 1
        exp = 1; // 在 IEEE 754 binary32 subnormal 隱含 exponent = 1 - bias (但這裡暫時設成1, 之後調整)
        while((mant & 0x00800000) == 0){ // 直到 bit23 成為 1
            mant <<= 1;
            exp--;                        // 相當於 exponent 減 1 (往更小)
        }
        mant &= 0x007FFFFF;              // 移除隱含 1 位 (bit23)
    }

    // 將 float exponent 轉成 half exponent: bias 差異 127 -> 15
    int32_t half_exp = (int32_t)exp - 127 + 15;

    // 溢出: 轉 Inf (不做 saturate)
    if(half_exp >= 0x1F){
        return (sign << 15) | 0x7C00; // Inf
    }

    // 可能成為 subnormal 或 0
    if(half_exp <= 0){
        // 若小到無法成為 subnormal (超過 10 bits 與 implicit 1 也移光) -> 直接為 0
        if(half_exp < -10){
            return (sign << 15);
        }
        // 形成帶有隱含 1 的 24-bit mantissa: 1.x * 2^(exp) -> mant 部分加上隱含位
        uint32_t m = mant | 0x00800000; // 加入 leading 1 (bit23)
        // shift = 1 - half_exp: 我們需要右移 (1 - half_exp) 次使 exponent = 0 並下放為 subnormal
        int shift = 1 - half_exp; // 1..10
        // 目標是得到 10-bit mantissa (無隱含 1) 以及 RNE 捨入
        // 先把需要保留的位數 (10) 左邊的 bits 取出
        // 我們想最後得到: m_sub = (m >> (shift + 13)) (因 float mant 有 23 bits, half mant 10 bits, 差 13)
        uint32_t shifted = m >> (shift + 13); // 初步 10-bit (含可能進位前)
        uint32_t rem_mask = (1u << (shift + 13)) - 1; // 被捨去的所有 bits
        uint32_t rem = m & rem_mask;                  // remainder bits
        uint32_t halfway = 1u << (shift + 12);        // halfway bit (guard 位置)
        uint32_t lsb = shifted & 1u;                  // 最低保留位，用於 ties-to-even
        if(rem > halfway || (rem == halfway && lsb)){
            shifted++;
        }
        return (sign << 15) | (uint16_t)shifted; // exp=0 已包含在格式中
    }

    // Normal case
    // 取得 10-bit mantissa 與保留捨去區 (13 bits) -> 使用 RNE
    uint32_t base = mant >> 13;              // 10 bits (可能進位前)
    uint32_t rem  = mant & 0x1FFF;           // 13 bits remainder
    uint32_t halfway = 0x1000;               // 中點 (bit12)
    uint32_t lsb = base & 1u;
    if(rem > halfway || (rem == halfway && lsb)){
        base++;
        if(base == 0x400){ // mantissa overflow (從 0x3FF + 1 -> 0x400)
            base = 0;      // mantissa rollover
            half_exp++;
            if(half_exp >= 0x1F){ // 進位造成 exponent 溢出 -> Inf
                return (sign << 15) | 0x7C00;
            }
        }
    }

    return (sign << 15) | ((half_exp & 0x1F) << 10) | (uint16_t)(base & 0x3FF);
}

static float fp16_to_float(uint16_t h){
    uint32_t sign = (h>>15)&1;
    uint32_t exp = (h>>10)&0x1F;
    uint32_t mant = h & 0x3FF;
    uint32_t out_sign = sign<<31;
    if(exp==0){
        if(mant==0){ uint32_t z=out_sign; float f; std::memcpy(&f,&z,4); return f; }
        // subnormal
        float m = mant / 1024.0f;
        float v = std::ldexp(m, -14); // 2^-14 * m
        return sign? -v : v;
    } else if(exp==0x1F){
        if(mant==0){ uint32_t inf = out_sign | 0x7F800000; float f; std::memcpy(&f,&inf,4); return f; }
        uint32_t nan = out_sign | 0x7FC00000; float f; std::memcpy(&f,&nan,4); return f;
    }
    int32_t full_exp = (int32_t)exp - 15 + 127;
    uint32_t full_mant = mant << 13;
    uint32_t bits = out_sign | ((full_exp & 0xFF)<<23) | full_mant;
    float f; std::memcpy(&f,&bits,4); return f;
}

float fp16_trunc(float fp32) {
    // 將 fp32 量化到 fp16 再轉回 (模擬硬體半精度來源運算數)
    uint16_t h = float_to_fp16(fp32);
    float f = fp16_to_float(h);
    return f;
}