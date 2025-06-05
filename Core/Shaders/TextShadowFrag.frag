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

float GetAlpha( vec2 uv, float range )
{
    return clamp(texture(SignedDistanceFieldTex, uv).x * range + 0.5, 0, 1);
}

void main()
{
    float alpha1 = GetAlpha(Tex, HeightRange) * Color.a;
    float alpha2 = GetAlpha(Tex - ShadowOffset, HeightRange * ShadowHardness) * ShadowOpacity * Color.a;
    outColor = vec4( Color.rgb * alpha1, mix(alpha2, 1, alpha1) );
}
