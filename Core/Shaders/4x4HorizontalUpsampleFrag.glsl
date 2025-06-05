#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D ColorTex;

layout(location = 0) in vec2 Tex;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Constants
{
    uvec2 TextureSize;  // source texture size
    float kA;  // Controls sharpness
};

vec3 GetColor(uint s, uint t)
{
#ifdef GAMMA_SPACE
    return ApplyDisplayProfile(texelFetch(ColorTex, ivec2(s, t), 0).rgb, DISPLAY_PLANE_FORMAT);
#else
    return texelFetch(ColorTex, ivec2(s, t), 0).rgb;
#endif
}

void main()
{
    vec2 t = Tex * TextureSize + 0.5;
    vec2 f = fract(t);
    ivec2 st = ivec2(t.x, gl_FragCoord.y);

    uint MaxWidth = TextureSize.x - 1;

    uint s0 = max(st.x - 2, 0);
    uint s1 = max(st.x - 1, 0);
    uint s2 = min(st.x + 0, MaxWidth);
    uint s3 = min(st.x + 1, MaxWidth);

    vec4 W = GetUpsampleFilterWeights(f.x, kA);

    mat4x3 Colors = 
    mat4x3(
        GetColor(s0, st.y),
        GetColor(s1, st.y),
        GetColor(s2, st.y),
        GetColor(s3, st.y)
    );
    outColor = vec4(Colors * W, 1.0);
//    outColor = vec4(Color, 1.0);
}