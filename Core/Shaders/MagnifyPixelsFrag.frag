#version 450

#extension GL_GOOGLE_include_directive : enable
#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Constants
{
    float ScaleFactor;
};

void main()
{
    // move uv center to origin, scale then move back
    vec2 scaledUV = ScaleFactor * (uv - 0.5) + 0.5;
    outColor = texture(texSampler, scaledUV);
}
