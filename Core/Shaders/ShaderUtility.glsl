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

#ifndef _SHADER_UTILITY_HLSLI_
#define _SHADER_UTILITY_HLSLI_

#include "ColorSpaceUtility.glsl"
//#include "PixelPacking.hlsli"

// Encodes a smooth logarithmic gradient for even distribution of precision natural to vision
float LinearToLogLuminance( float x, float gamma)
{
    return log2(mix(1, exp2(gamma), x)) / gamma;
}

// This assumes the default color gamut found in sRGB and REC709.  The color primaries determine these
// coefficients.  Note that this operates on linear values, not gamma space.
float RGBToLuminance( vec3 x )
{
    return dot( x, vec3(0.212671, 0.715160, 0.072169) );        // Defined by sRGB/Rec.709 gamut
}

float MaxChannel(vec3 x)
{
    return max(x.x, max(x.y, x.z));
}

// This is the same as above, but converts the linear luminance value to a more subjective "perceived luminance",
// which could be called the Log-Luminance.
float RGBToLogLuminance( vec3 x, float gamma)
{
    return LinearToLogLuminance( RGBToLuminance(x), gamma );
}

// A fast invertible tone map that preserves color (Reinhard)
vec3 TM( vec3 rgb )
{
    return rgb / (1 + RGBToLuminance(rgb));
}

// Inverse of preceding function
vec3 ITM( vec3 rgb )
{
    return rgb / (1 - RGBToLuminance(rgb));
}

// 8-bit should range from 16 to 235
vec3 RGBFullToLimited8bit( vec3 x )
{
    return clamp(x, 0.0, 1.0) * 219.0 / 255.0 + 16.0 / 255.0;
}

vec3 RGBLimitedToFull8bit( vec3 x )
{
    return clamp((x - 16.0 / 255.0) * 255.0 / 219.0, 0.0, 1.0);
}

// 10-bit should range from 64 to 940
vec3 RGBFullToLimited10bit( vec3 x )
{
    return clamp(x, 0.0, 1.0) * 876.0 / 1023.0 + 64.0 / 1023.0;
}

vec3 RGBLimitedToFull10bit( vec3 x )
{
    return clamp((x - 64.0 / 1023.0) * 1023.0 / 876.0, 0.0, 1.0);
}

#define COLOR_FORMAT_LINEAR            0
#define COLOR_FORMAT_sRGB_FULL        1
#define COLOR_FORMAT_sRGB_LIMITED    2
#define COLOR_FORMAT_Rec709_FULL    3
#define COLOR_FORMAT_Rec709_LIMITED    4
#define COLOR_FORMAT_HDR10            5
#define COLOR_FORMAT_TV_DEFAULT        COLOR_FORMAT_Rec709_LIMITED
#define COLOR_FORMAT_PC_DEFAULT        COLOR_FORMAT_sRGB_FULL

#define HDR_COLOR_FORMAT            COLOR_FORMAT_LINEAR
#define LDR_COLOR_FORMAT            COLOR_FORMAT_LINEAR
#define DISPLAY_PLANE_FORMAT        COLOR_FORMAT_PC_DEFAULT

vec3 ApplyDisplayProfile( vec3 x, int DisplayFormat )
{
    switch (DisplayFormat)
    {
    default:
    case COLOR_FORMAT_LINEAR:
        return x;
    case COLOR_FORMAT_sRGB_FULL:
        return ApplySRGBCurve(x);
    case COLOR_FORMAT_sRGB_LIMITED:
        return RGBFullToLimited10bit(ApplySRGBCurve(x));
    case COLOR_FORMAT_Rec709_FULL:
        return ApplyREC709Curve(x);
    case COLOR_FORMAT_Rec709_LIMITED:
        return RGBFullToLimited10bit(ApplyREC709Curve(x));
    case COLOR_FORMAT_HDR10:
        return ApplyREC2084Curve(REC709toREC2020(x));
    };
}

vec3 RemoveDisplayProfile( vec3 x, int DisplayFormat )
{
    switch (DisplayFormat)
    {
    default:
    case COLOR_FORMAT_LINEAR:
        return x;
    case COLOR_FORMAT_sRGB_FULL:
        return RemoveSRGBCurve(x);
    case COLOR_FORMAT_sRGB_LIMITED:
        return RemoveSRGBCurve(RGBLimitedToFull10bit(x));
    case COLOR_FORMAT_Rec709_FULL:
        return RemoveREC709Curve(x);
    case COLOR_FORMAT_Rec709_LIMITED:
        return RemoveREC709Curve(RGBLimitedToFull10bit(x));
    case COLOR_FORMAT_HDR10:
        return REC2020toREC709(RemoveREC2084Curve(x));
    };
}

vec3 ConvertColor( vec3 x, int FromFormat, int ToFormat )
{
    if (FromFormat == ToFormat)
        return x;

    return ApplyDisplayProfile(RemoveDisplayProfile(x, FromFormat), ToFormat);
}

#endif // _SHADER_UTILITY_HLSLI_
