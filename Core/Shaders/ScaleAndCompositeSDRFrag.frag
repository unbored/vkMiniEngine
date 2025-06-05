#version 450

#extension GL_GOOGLE_include_directive : enable
#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform sampler2D overlaySampler;

// UVOffset = 0.7071f / g_NativeWidth, 0.7071f / g_NativeHeight,
// which means 0.7071 px offset.
layout(push_constant) uniform Constants
{
    vec2 UVOffset;
};

layout(location = 0) in vec2 Tex;

layout(location = 0) out vec4 outColor;

vec3 SampleColor(vec2 uv)
{
    return texture(texSampler, uv).rgb;
}

vec3 ScaleBuffer(vec2 uv)
{
    // a sharpen filter
    return 1.4 * SampleColor(uv) - 0.1 * (
        SampleColor(uv + vec2(+UVOffset.x, +UVOffset.y))
        + SampleColor(uv + vec2(+UVOffset.x, -UVOffset.y))
        + SampleColor(uv + vec2(-UVOffset.x, +UVOffset.y))
        + SampleColor(uv + vec2(-UVOffset.x, -UVOffset.y))
        );
}

void main()
{
    vec3 rgb = ScaleBuffer(Tex);
    vec3 texColor = ApplyDisplayProfile(rgb, DISPLAY_PLANE_FORMAT);
    vec4 overlayColor = texture(overlaySampler, Tex);
    outColor = vec4(overlayColor.rgb + texColor * (1.0 - overlayColor.a), 1.0);
}
