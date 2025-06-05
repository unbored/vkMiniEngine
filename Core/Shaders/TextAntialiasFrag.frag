#version 450

layout (push_constant) uniform FontParamsFrag
{
layout(offset = 48)
    vec4 Color;
    vec2 ShadowOffset;
    float ShadowHardness;
    float ShadowOpacity;
    float HeightRange;    // The range of the signed distance field.
};

layout(binding = 0) uniform sampler2D SignedDistanceFieldTex;

layout(location = 0) in vec2 Tex;

layout(location = 0) out vec4 outColor;

float GetAlpha( vec2 uv )
{
    return clamp(texture(SignedDistanceFieldTex, uv).x * HeightRange + 0.5, 0, 1);
}

void main()
{
    outColor = vec4(Color.rgb, 1) * GetAlpha(Tex) * Color.a;
    outColor = vec4(1.0);
}
