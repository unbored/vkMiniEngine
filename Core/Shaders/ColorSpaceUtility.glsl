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

#ifndef _COLOR_SPACE_UTILITY_GLSL_
#define _COLOR_SPACE_UTILITY_GLSL_

//
// Gamma ramps and encoding transfer functions
//
// Orthogonal to color space though usually tightly coupled.  For instance, sRGB is both a
// color space (defined by three basis vectors and a white point) and a gamma ramp.  Gamma
// ramps are designed to reduce perceptual error when quantizing floats to integers with a
// limited number of bits.  More variation is needed in darker colors because our eyes are
// more sensitive in the dark.  The way the curve helps is that it spreads out dark values
// across more code words allowing for more variation.  Likewise, bright values are merged
// together into fewer code words allowing for less variation.
//
// The sRGB curve is not a true gamma ramp but rather a piecewise function comprising a linear
// section and a power function.  When sRGB-encoded colors are passed to an LCD monitor, they
// look correct on screen because the monitor expects the colors to be encoded with sRGB, and it
// removes the sRGB curve to linearize the values.  When textures are encoded with sRGB--as many
// are--the sRGB curve needs to be removed before involving the colors in linear mathematics such
// as physically based lighting.

vec3 ApplySRGBCurve( vec3 x )
{
    // Approximately pow(x, 1.0 / 2.2)
    // return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
    return mix(12.92 * x, 1.055 * pow(x, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, x));
}

vec3 RemoveSRGBCurve( vec3 x )
{
    // Approximately pow(x, 2.2)
//    return x < 0.04045 ? x / 12.92 : pow( (x + 0.055) / 1.055, 2.4 );
    return mix(x / 12.92, pow( (x + 0.055) / 1.055, vec3(2.4) ), step(0.04045, x));
}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
vec3 ApplySRGBCurve_Fast( vec3 x )
{
//    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
    return mix(12.92 * x, 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719, step(0.0031308, x));
}

vec3 RemoveSRGBCurve_Fast( vec3 x )
{
//    return x < 0.04045 ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
    return mix(x / 12.92, -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864, step(0.04045, x));
}

// The OETF recommended for content shown on HDTVs.  This "gamma ramp" may increase contrast as
// appropriate for viewing in a dark environment.  Always use this curve with Limited RGB as it is
// used in conjunction with HDTVs.
vec3 ApplyREC709Curve( vec3 x )
{
//    return x < 0.0181 ? 4.5 * x : 1.0993 * pow(x, 0.45) - 0.0993;
    return mix(4.5 * x, 1.0993 * pow(x, vec3(0.45)) - 0.0993, step(0.0181, x));
}

vec3 RemoveREC709Curve( vec3 x )
{
//    return x < 0.08145 ? x / 4.5 : pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
    return mix(x / 4.5, pow((x + 0.0993) / 1.0993, vec3(1.0 / 0.45)), step(0.08145, x));
}

// This is the new HDR transfer function, also called "PQ" for perceptual quantizer.  Note that REC2084
// does not also refer to a color space.  REC2084 is typically used with the REC2020 color space.
vec3 ApplyREC2084Curve(vec3 L)
{
    float m1 = 2610.0 / 4096.0 / 4.0;
    float m2 = 2523.0 / 4096.0 * 128.0;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32.0;
    float c3 = 2392.0 / 4096.0 * 32.0;
    vec3 Lp = pow(L, vec3(m1));
    return pow((c1 + c2 * Lp) / (1.0 + c3 * Lp), vec3(m2));
}

vec3 RemoveREC2084Curve(vec3 N)
{
    float m1 = 2610.0 / 4096.0 / 4.0;
    float m2 = 2523.0 / 4096.0 * 128.0;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32.0;
    float c3 = 2392.0 / 4096.0 * 32.0;
    vec3 Np = pow(N, vec3(1.0 / m2));
    return pow(max(Np - c1, 0) / (c2 - c3 * Np), vec3(1.0 / m1));
}

//
// Color space conversions
//
// These assume linear (not gamma-encoded) values.  A color space conversion is a change
// of basis (like in Linear Algebra).  Since a color space is defined by three vectors--
// the basis vectors--changing space involves a matrix-vector multiplication.  Note that
// changing the color space may result in colors that are "out of bounds" because some
// color spaces have larger gamuts than others.  When converting some colors from a wide
// gamut to small gamut, negative values may result, which are inexpressible in that new
// color space.
//
// It would be ideal to build a color pipeline which never throws away inexpressible (but
// perceivable) colors.  This means using a color space that is as wide as possible.  The
// XYZ color space is the neutral, all-encompassing color space, but it has the unfortunate
// property of having negative values (specifically in X and Z).  To correct this, a further
// transformation can be made to X and Z to make them always positive.  They can have their
// precision needs reduced by dividing by Y, allowing X and Z to be packed into two UNORM8s.
// This color space is called YUV for lack of a better name.
//

// Note:  Rec.709 and sRGB share the same color primaries and white point.  Their only difference
// is the transfer curve used.

vec3 REC709toREC2020( vec3 RGB709 )
{
    const mat3 ConvMat =
    mat3(
        0.627402, 0.329292, 0.043306,
        0.069095, 0.919544, 0.011360,
        0.016394, 0.088028, 0.895578
    );
    return ConvMat * RGB709;
}

vec3 REC2020toREC709(vec3 RGB2020)
{
    const mat3 ConvMat =
    mat3(
        1.660496, -0.587656, -0.072840,
        -0.124547, 1.132895, -0.008348,
        -0.018154, -0.100597, 1.118751
    );
    return ConvMat * RGB2020;
}

vec3 REC709toDCIP3( vec3 RGB709 )
{
    const mat3 ConvMat =
    mat3(
        0.822458, 0.177542, 0.000000,
        0.033193, 0.966807, 0.000000,
        0.017085, 0.072410, 0.910505
    );
    return ConvMat * RGB709;
}

vec3 DCIP3toREC709( vec3 RGBP3 )
{
    const mat3 ConvMat =
    mat3(
        1.224947, -0.224947, 0.000000,
        -0.042056, 1.042056, 0.000000,
        -0.019641, -0.078651, 1.098291
    );
    return ConvMat * RGBP3;
}

#endif // _COLOR_SPACE_UTILITY_GLSL_
