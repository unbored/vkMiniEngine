#version 450

#extension GL_GOOGLE_include_directive : enable
#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Constants
{
    vec2 UVOffset0;   // Four UV offsets to determine the sharpening tap locations
    vec2 UVOffset1;   // We get two more offsets by negating these two.
    float WA, WB;       // Sharpness strength:  WB = 1.0 + Sharpness; 4*WA + WB = 1
};

vec3 GetColor(vec2 UV)
{
    vec3 Color = texture(texSampler, UV).rgb;
#ifdef GAMMA_SPACE
    return ApplyDisplayProfile(Color, DISPLAY_PLANE_FORMAT);
#else
    return Color;
#endif
}

void main()
{
    vec3 Color = WB * GetColor(uv) - WA * (
        GetColor(uv + UVOffset0) + GetColor(uv - UVOffset0) +
        GetColor(uv + UVOffset1) + GetColor(uv - UVOffset1));

#ifdef GAMMA_SPACE
    outColor = vec4(Color, 1.0);
#else
    outColor = vec4(ApplyDisplayProfile(Color, DISPLAY_PLANE_FORMAT), 1.0);
#endif
}
