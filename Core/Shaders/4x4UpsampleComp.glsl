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
//

#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D Source;
layout(binding = 2, rgba8) uniform writeonly image2D Dest;

layout(push_constant) uniform Constants
{
    vec2 kRcpScale; // src/dest
    float kA;  // Controls sharpness
};

// TILE in dest space, SAMPLE in src space
// implement assure TILE_DIM = WorkGroupSize
#ifndef ENABLE_FAST_PATH
    #define TILE_DIM_X 16
    #define TILE_DIM_Y 16
#endif
#define GROUP_COUNT (TILE_DIM_X * TILE_DIM_Y)

layout (local_size_x = TILE_DIM_X, local_size_y = TILE_DIM_Y) in;

// The fast path can be enabled when the source tile plus the extra border pixels fit
// within the destination tile size.  For 16x16 destination tiles and 4 taps, you can
// upsample 13x13 tiles and smaller using the fast path.  Src/Dest <= 13/16 --> FAST
#ifdef ENABLE_FAST_PATH
    #define SAMPLES_X TILE_DIM_X
    #define SAMPLES_Y TILE_DIM_Y
#else
    #define SAMPLES_X (TILE_DIM_X + 3)
    #define SAMPLES_Y (TILE_DIM_Y + 3)
#endif

#define TOTAL_SAMPLES (SAMPLES_X * SAMPLES_Y)

// De-interleaved to avoid LDS bank conflicts
shared float g_R[TOTAL_SAMPLES];
shared float g_G[TOTAL_SAMPLES];
shared float g_B[TOTAL_SAMPLES];

// Store pixel to LDS (local data store)
void StoreLDS(uint LdsIdx, vec3 rgb)
{
    g_R[LdsIdx] = rgb.r;
    g_G[LdsIdx] = rgb.g;
    g_B[LdsIdx] = rgb.b;
}

// Load four pixel samples from LDS.  Stride determines horizontal or vertical groups.
mat4x3 LoadSamples(uint idx, uint Stride)
{
    uint i0 = idx, i1 = idx+Stride, i2 = idx+2*Stride, i3=idx+3*Stride;
    return mat4x3(
        g_R[i0], g_G[i0], g_B[i0],
        g_R[i1], g_G[i1], g_B[i1],
        g_R[i2], g_G[i2], g_B[i2],
        g_R[i3], g_G[i3], g_B[i3]);
}

void main()
{
#ifndef ENABLE_FAST_PATH
    // Number of samples needed from the source buffer to generate the output tile dimensions.
    // if src/dest<=13/16, SampleSpace<=16x16, which fast path enabled
    const uvec2 SampleSpace = uvec2(ceil(vec2(TILE_DIM_X, TILE_DIM_Y) * kRcpScale + 3.0));
#endif

    // Pre-Load source pixels
    // plus 0.5 to locate center of pixel. mapping work groups to source space
    ivec2 UpperLeft = ivec2(floor((gl_WorkGroupID.xy * uvec2(TILE_DIM_X, TILE_DIM_Y) + 0.5) * kRcpScale - 1.5));
#ifdef ENABLE_FAST_PATH
    // take one pixel
    // NOTE: If bandwidth is more of a factor than ALU, uncomment this condition.
    //if (all(GTid.xy < SampleSpace))
        StoreLDS(gl_LocalInvocationIndex, texelFetch(Source, UpperLeft + ivec2(gl_LocalInvocationID.xy), 0).rgb);
#else
    for (uint i = gl_LocalInvocationIndex; i < TOTAL_SAMPLES; i += GROUP_COUNT)
        StoreLDS(i, texelFetch(Source, UpperLeft + ivec2(i % SAMPLES_X, i / SAMPLES_Y), 0).rgb);
#endif

    barrier();

    // The coordinate of the top-left sample from the 4x4 kernel (offset by -0.5
    // so that whole numbers land on a pixel center.)  This is in source texture space.
    vec2 TopLeftSample = (gl_GlobalInvocationID.xy + 0.5) * kRcpScale - 1.5;

    // Position of samples relative to pixels used to evaluate the Sinc function.
    vec2 Phase = fract(TopLeftSample);

    // LDS tile coordinate for the top-left sample (for this thread)
    uvec2 TileST = ivec2(floor(TopLeftSample)) - UpperLeft;

    // Convolution weights, one per sample (in each dimension)
    vec4 xWeights = GetUpsampleFilterWeights(Phase.x, kA);
    vec4 yWeights = GetUpsampleFilterWeights(Phase.y, kA);

    // Horizontally convolve the first N rows
    uint ReadIdx = TileST.x + gl_LocalInvocationID.y * SAMPLES_X;
#ifdef ENABLE_FAST_PATH
    StoreLDS(gl_LocalInvocationIndex, LoadSamples(ReadIdx, 1) * xWeights);
#else
    uint WriteIdx = gl_LocalInvocationID.x + gl_LocalInvocationID.y * SAMPLES_X;
    StoreLDS(WriteIdx, LoadSamples(ReadIdx, 1) * xWeights);

    // If the source tile plus border is larger than the destination tile, we
    // have to convolve a few more rows.
    if (gl_LocalInvocationIndex + GROUP_COUNT < SampleSpace.y * TILE_DIM_X)
    {
        ReadIdx += TILE_DIM_Y * SAMPLES_X;
        WriteIdx += TILE_DIM_Y * SAMPLES_X;
        StoreLDS(WriteIdx, LoadSamples(ReadIdx, 1) * xWeights);
    }
#endif

    barrier();

    // Convolve vertically N columns
    ReadIdx = gl_LocalInvocationID.x + TileST.y * SAMPLES_X;
    vec3 Result = LoadSamples(ReadIdx, SAMPLES_X) * yWeights;

    // Transform to display settings
    Result = RemoveDisplayProfile(Result, LDR_COLOR_FORMAT);
    imageStore(Dest, ivec2(gl_GlobalInvocationID.xy), vec4(ApplyDisplayProfile(Result, DISPLAY_PLANE_FORMAT), 1.0));
    // imageStore(Dest, ivec2(gl_GlobalInvocationID.xy), vec4(1.0));
}
