#version 450

#extension GL_GOOGLE_include_directive : enable
#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 Tex;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 rgb = texture(texSampler, Tex).rgb;
    // here we convert to sRGB format
    outColor = vec4(ApplyDisplayProfile(rgb, DISPLAY_PLANE_FORMAT), 1.0);
}
