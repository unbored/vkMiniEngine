#version 450

#extension GL_GOOGLE_include_directive : enable
#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform sampler2D overlaySampler;

layout(location = 0) in vec2 Tex;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 rgb = texture(texSampler, Tex).rgb;
    vec3 texColor = ApplyDisplayProfile(rgb, DISPLAY_PLANE_FORMAT);
    vec4 overlayColor = texture(overlaySampler, Tex);
    outColor = vec4(overlayColor.rgb + texColor * (1.0 - overlayColor.a), 1.0);
}
