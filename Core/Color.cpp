//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "Color.h"

uint32_t Color::R11G11B10F(bool RoundToEven) const
{
    static const float kMaxVal = float(1 << 16);
    static const float kF32toF16 = (1.0 / (1ull << 56)) * (1.0 / (1ull << 56));

    union { float f; uint32_t u; } R, G, B;

    R.f = glm::clamp(m_value.r, 0.0f, kMaxVal) * kF32toF16;
    G.f = glm::clamp(m_value.g, 0.0f, kMaxVal) * kF32toF16;
    B.f = glm::clamp(m_value.b, 0.0f, kMaxVal) * kF32toF16;

    if (RoundToEven)
    {
        // Bankers rounding:  2.5 -> 2.0  ;  3.5 -> 4.0
        R.u += 0x0FFFF + ((R.u >> 16) & 1);
        G.u += 0x0FFFF + ((G.u >> 16) & 1);
        B.u += 0x1FFFF + ((B.u >> 17) & 1);
    }
    else
    {
        // Default rounding:  2.5 -> 3.0  ;  3.5 -> 4.0
        R.u += 0x00010000;
        G.u += 0x00010000;
        B.u += 0x00020000;
    }

    R.u &= 0x0FFE0000;
    G.u &= 0x0FFE0000;
    B.u &= 0x0FFC0000;

    return R.u >> 17 | G.u >> 6 | B.u << 4;
}

uint32_t Color::R9G9B9E5() const
{
    static const float kMaxVal = float(0x1FF << 7);
    static const float kMinVal = float(1.f / (1 << 16));

    // Clamp RGB to [0, 1.FF*2^16]
    float r = glm::clamp(m_value.r, 0.0f, kMaxVal);
    float g = glm::clamp(m_value.g, 0.0f, kMaxVal);
    float b = glm::clamp(m_value.b, 0.0f, kMaxVal);

    // Compute the maximum channel, no less than 1.0*2^-15
    float MaxChannel = glm::max(glm::max(r, g), glm::max(b, kMinVal));

    // Take the exponent of the maximum channel (rounding up the 9th bit) and
    // add 15 to it.  When added to the channels, it causes the implicit '1.0'
    // bit and the first 8 mantissa bits to be shifted down to the low 9 bits
    // of the mantissa, rounding the truncated bits.
    union { float f; int32_t i; } R, G, B, E;
    E.f = MaxChannel;
    E.i += 0x07804000; // Add 15 to the exponent and 0x4000 to the mantissa
    E.i &= 0x7F800000; // Zero the mantissa

    // This shifts the 9-bit values we need into the lowest bits, rounding as
    // needed.  Note that if the channel has a smaller exponent than the max
    // channel, it will shift even more.  This is intentional.
    R.f = r + E.f;
    G.f = g + E.f;
    B.f = b + E.f;

    // Convert the Bias to the correct exponent in the upper 5 bits.
    E.i <<= 4;
    E.i += 0x10000000;

    // Combine the fields.  RGB floats have unwanted data in the upper 9
    // bits.  Only red needs to mask them off because green and blue shift
    // it out to the left.
    return E.i | B.i << 18 | G.i << 9 | R.i & 511;
}
