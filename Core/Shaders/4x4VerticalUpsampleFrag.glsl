#include "ShaderUtility.glsl"

layout(binding = 0) uniform sampler2D ColorTex;

layout(location = 0) in vec2 Tex;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Constants
{
    uvec2 TextureSize;  // dest width, source height
    float kA;  // Controls sharpness
};

vec3 GetColor(uint s, uint t)
{
    return texelFetch(ColorTex, ivec2(s, t), 0).rgb;
}

void main()
{
    vec2 t = Tex * TextureSize + 0.5;
    vec2 f = fract(t);
    ivec2 st = ivec2(gl_FragCoord.x, t.y);

    uint MaxHeight = TextureSize.y - 1;

    uint t0 = max(st.y - 2, 0);
    uint t1 = max(st.y - 1, 0);
    uint t2 = min(st.y + 0, MaxHeight);
    uint t3 = min(st.y + 1, MaxHeight);

    vec4 W = GetUpsampleFilterWeights(f.y, kA);

    mat4x3 Colors = 
    mat4x3(
        GetColor(st.x, t0),
        GetColor(st.x, t1),
        GetColor(st.x, t2),
        GetColor(st.x, t3)
    );

#ifdef GAMMA_SPACE
    outColor = vec4(Colors * W, 1.0);
#else
    outColor = vec4(ApplyDisplayProfile(Colors * W, DISPLAY_PLANE_FORMAT), 1.0);
#endif
}
