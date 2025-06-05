#version 450

#extension GL_GOOGLE_include_directive : enable
#include "Common.glsl"

layout(binding = kMaterialConstants) uniform FragConstants
{
	float TextureLevel;
};

layout (binding = kCommonCubeSamplers) uniform samplerCube radianceIBLTexture;

layout (location = 0) in vec3 viewDir;

layout (location = 0) out vec4 outColor;

void main()
{
	outColor = textureLod(radianceIBLTexture, viewDir, TextureLevel);
}