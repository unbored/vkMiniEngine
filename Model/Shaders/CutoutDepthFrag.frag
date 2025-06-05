#version 450

#extension GL_GOOGLE_include_directive : enable
#include "Common.glsl"

layout (binding = kMaterialSamplers) uniform sampler2D baseColorTexture;

layout (binding = kMaterialConstants) uniform MaterialConstants
{
    vec4 baseColorFactor;
    vec3 emissiveFactor;
    float normalTextureScale;
    vec2 metallicRoughnessFactor;
    uint flags;
};

layout (location = 0) in vec2 uv;

void main()
{
	float cutoff = unpackHalf2x16(flags).x;
	if (texture(baseColorTexture, uv).a < cutoff)
	{
		discard;
	}
}