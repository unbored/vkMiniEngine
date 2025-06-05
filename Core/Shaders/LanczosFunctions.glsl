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
// Author(s):  James Stanard
//             Jack Elliott
//

//float Sinc(float x)
//{
//    if (x == 0.0)
//        return 1.0;
//    else
//        return sin(x) / x;
//}
//
//float Lanczos(float x, float a)
//{
//    static const float M_PI = 3.1415926535897932384626433832795;
//    return abs(x) < a ? Sinc(x*M_PI) * Sinc(x/a*M_PI) : 0.0;
//}

//float4 ComputeWeights(float fracPart)
//{
//    float4 weights;
//    weights.x = Lanczos(-1.0 - fracPart, 2.0);
//    weights.y = Lanczos( 0.0 - fracPart, 2.0);
//    weights.z = Lanczos( 1.0 - fracPart, 2.0);
//    weights.w = Lanczos( 2.0 - fracPart, 2.0);
//    return weights / dot(weights, 1);
//}

const float M_PI = 3.1415926535897932384626433832795;

// in this case x will never be 0
#define Sinc(x) (sin(x) / (x))

#define Lanczos(x, a) mix(Sinc((x)*M_PI) * Sinc((x)/(a)*M_PI), 0, step((a), abs(x)))

#define ComputeWeights(fracPart, a) vec4( \
    Lanczos(-1.0 - (fracPart), (a)), \
    Lanczos(0.0 - (fracPart), (a)), \
    Lanczos(1.0 - (fracPart), (a)), \
    Lanczos(2.0 - (fracPart), (a)))

const vec4 weights[16] = vec4[]
    (
        ComputeWeights( 0.5 / 16.0, 2),
        ComputeWeights( 1.5 / 16.0, 2),
        ComputeWeights( 2.5 / 16.0, 2),
        ComputeWeights( 3.5 / 16.0, 2),
        ComputeWeights( 4.5 / 16.0, 2),
        ComputeWeights( 5.5 / 16.0, 2),
        ComputeWeights( 6.5 / 16.0, 2),
        ComputeWeights( 7.5 / 16.0, 2),
        ComputeWeights( 8.5 / 16.0, 2),
        ComputeWeights( 9.5 / 16.0, 2),
        ComputeWeights(10.5 / 16.0, 2),
        ComputeWeights(11.5 / 16.0, 2),
        ComputeWeights(12.5 / 16.0, 2),
        ComputeWeights(13.5 / 16.0, 2),
        ComputeWeights(14.5 / 16.0, 2),
        ComputeWeights(15.5 / 16.0, 2)
    );

const vec4 FilterWeights[16] = vec4[]
    (
        weights[0] / dot(weights[0], vec4(1)),
        weights[1] / dot(weights[1], vec4(1)),
        weights[2] / dot(weights[2], vec4(1)),
        weights[3] / dot(weights[3], vec4(1)),
        weights[4] / dot(weights[4], vec4(1)),
        weights[5] / dot(weights[5], vec4(1)),
        weights[6] / dot(weights[6], vec4(1)),
        weights[7] / dot(weights[7], vec4(1)),
        weights[8] / dot(weights[8], vec4(1)),
        weights[9] / dot(weights[9], vec4(1)),
        weights[10] / dot(weights[10], vec4(1)),
        weights[11] / dot(weights[11], vec4(1)),
        weights[12] / dot(weights[12], vec4(1)),
        weights[13] / dot(weights[13], vec4(1)),
        weights[14] / dot(weights[14], vec4(1)),
        weights[15] / dot(weights[15], vec4(1))
    );

vec4 GetUpsampleFilterWeights(float offset, float A)
{
    return FilterWeights[uint(offset * 16.0)];
}
