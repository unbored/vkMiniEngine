#include "Common.glsl"

layout (binding = kMaterialSamplers) uniform sampler2D matTex[5];
#define baseColorTexture matTex[0]
#define metallicRoughnessTexture matTex[1]
#define occlusionTexture matTex[2]
#define emissiveTexture matTex[3]
#define normalTexture matTex[4]

layout (binding = kCommonSamplers) uniform samplerCube cubeTex[];
layout (binding = kCommonSamplers) uniform sampler2D comTex[];
layout (binding = kCommonSamplers) uniform sampler2DShadow shadowTex[];
#define radianceIBLTexture cubeTex[0]
#define irradianceIBLTexture cubeTex[1]
#define texSSAO comTex[2]
#define texSunShadow shadowTex[3]

layout (binding = kMaterialConstants) uniform MaterialConstants
{
    vec4 baseColorFactor;
    vec3 emissiveFactor;
    float normalTextureScale;
    vec2 metallicRoughnessFactor;
    uint flags;
};

layout (binding = kCommonConstants) uniform GlobalConstants
{
    mat4 ViewProj;
    mat4 SunShadowMatrix;
    vec3 ViewerPos;
    vec3 SunDirection;
    vec3 SunIntensity;
    float IBLRange;
    float IBLBias;
};

layout (location = 0) in VertOutput
{
	vec3 normal;
#ifndef NO_TANGENT_FRAME
	vec4 tangent;
#endif
	vec2 uv0;
#ifndef NO_SECOND_UV
	vec2 uv1;
#endif
	vec3 worldPos;
	vec3 sunShadowCoord;
} vertOutput;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265;
const vec3 kDielectricSpecular = vec3(0.04);

// Flag helpers
const uint BASECOLOR = 0;
const uint METALLICROUGHNESS = 1;
const uint OCCLUSION = 2;
const uint EMISSIVE = 3;
const uint NORMAL = 4;
#ifdef NO_SECOND_UV
#define UVSET( offset ) vertOutput.uv0
#else
#define UVSET( offset ) mix(vertOutput.uv0, vertOutput.uv1, (flags >> offset) & 1)
#endif

struct SurfaceProperties
{
    vec3 N;
    vec3 V;
    vec3 c_diff;
    vec3 c_spec;    // F0
    float roughness;
    float alpha; // roughness squared
    float alphaSqr; // alpha squared
    float NdotV;
};

struct LightProperties
{
    vec3 L;
    float NdotL;
    float LdotH;
    float NdotH;
};

float Pow5(float x)
{
    return x * x * x * x * x;
}

vec3 Fresnel_Schlick(vec3 F0, vec3 F90, float cosine)
{
    return mix(F0, F90, Pow5(1.0 - cosine));
}

float Fresnel_Schlick(float F0, float F90, float cosine)
{
    return mix(F0, F90, Pow5(1.0 - cosine));
}

float G_GGX(float k, float cosine)
{
    return cosine / max(1e-6, mix(k, 1, cosine));
}

// Specular G
float G_GGX_Schlick(SurfaceProperties surface, LightProperties light)
{
    float alpha = (surface.roughness + 1) * (surface.roughness + 1) / 4.0;
    float k = alpha / 2;
    return G_GGX(k, light.NdotL) * G_GGX(k, surface.NdotV);
}

// Specular F
vec3 Fresnel_Schlick(SurfaceProperties surface, LightProperties light)
{
    return Fresnel_Schlick(surface.c_spec, vec3(1.0), light.LdotH);
}

// Specular D
float D_TR(SurfaceProperties surface, LightProperties light)
{
    float a = mix(1, surface.alphaSqr, light.NdotH * light.NdotH);
    return surface.alphaSqr / max(1e-6, PI * a * a);
}

// Diffuse
vec3 Diffuse_Disney(SurfaceProperties surface, LightProperties light)
{
    float f90 = 0.5 + 2 * surface.roughness * light.LdotH * light.LdotH;
    return surface.c_diff / PI * Fresnel_Schlick(1.0, f90, light.NdotL) * Fresnel_Schlick(1.0, f90, surface.NdotV);
}

vec3 Diffuse_BRDF(SurfaceProperties surface, LightProperties light)
{
    return Diffuse_Disney(surface, light);
}

vec3 Specular_BRDF(SurfaceProperties surface, LightProperties light)
{
    float SD = D_TR(surface, light);
    vec3 SF = Fresnel_Schlick(surface, light);
    float SG = G_GGX_Schlick(surface, light);

    return SD * SF * SG / (4.0 * light.NdotL * surface.NdotV);
}

// Diffuse irradiance
vec3 Diffuse_IBL(SurfaceProperties Surface)
{
    // Assumption:  L = N

    //return Surface.c_diff * irradianceIBLTexture.Sample(defaultSampler, Surface.N);

    // This is nicer but more expensive, and specular can often drown out the diffuse anyway
    float LdotH = clamp(dot(Surface.N, normalize(Surface.N + Surface.V)), 0, 1);
    float fd90 = 0.5 + 2.0 * Surface.roughness * LdotH * LdotH;
    vec3 diffuse = Surface.c_diff / PI * Fresnel_Schlick(1, fd90, Surface.NdotV);
    return diffuse * texture(irradianceIBLTexture, Surface.N).rgb;
}

// Approximate specular IBL by sampling lower mips according to roughness.  Then modulate by Fresnel. 
vec3 Specular_IBL(SurfaceProperties Surface)
{
    float lod = Surface.roughness * IBLRange + IBLBias;
    vec3 specular = Fresnel_Schlick(Surface.c_spec, vec3(1), Surface.NdotV);
    return specular * textureLod(radianceIBLTexture, reflect(-Surface.V, Surface.N), lod).rgb;
}

vec3 ShadeDirectionalLight(SurfaceProperties surface, vec3 L, vec3 c_light)
{
    LightProperties light;
    light.L = L;

    // Half vector
    vec3 H = normalize(L + surface.V);

    // Pre-compute dot products
    light.NdotL = clamp(dot(surface.N, L), 0, 1);
    light.LdotH = clamp(dot(L, H), 0, 1);
    light.NdotH = clamp(dot(surface.N, H), 0, 1);

    // Diffuse & specular factors
    vec3 diffuse = Diffuse_BRDF(surface, light);
    vec3 specular = Specular_BRDF(surface, light);

    // Directional light
    return light.NdotL * c_light * (diffuse + specular);
}

vec3 ComputeNormal()
{
	vec3 normal = normalize(vertOutput.normal);
#ifdef NO_TANGENT_FRAME
    return normal;
#else
	// Construct tangent frame
    vec3 tangent = normalize(vertOutput.tangent.xyz);
    vec3 bitangent = normalize(cross(normal, tangent)) * vertOutput.tangent.w;
    mat3 tangentFrame = mat3(tangent, bitangent, normal);

	// Read normal map and convert to SNORM (TODO:  convert all normal maps to R8G8B8A8_SNORM?)
    normal = texture(normalTexture, UVSET(NORMAL)).xyz * 2.0 - 1.0;

    // glTF spec says to normalize N before and after scaling, but that's excessive
    normal = normalize(normal * vec3(normalTextureScale, normalTextureScale, 1));

    // Multiply by transpose (reverse order)
    return tangentFrame * normal;
#endif
}

void main()
{
    vec4 baseColor = baseColorFactor * texture(baseColorTexture, UVSET(BASECOLOR));
    vec2 metallicRoughness = metallicRoughnessFactor * texture(metallicRoughnessTexture, UVSET(METALLICROUGHNESS)).bg;
    float occlusion = texture(occlusionTexture, UVSET(OCCLUSION)).x;
    vec3 emissive = emissiveFactor * texture(emissiveTexture, UVSET(EMISSIVE)).xyz;
    // TODO: compute normal
    vec3 normal = ComputeNormal();

    SurfaceProperties surface;
    surface.N = normal;
    surface.V = normalize(ViewerPos - vertOutput.worldPos);
    surface.NdotV = clamp(dot(surface.N, surface.V), 0, 1);
    surface.c_diff = baseColor.rgb * (1 - kDielectricSpecular) * (1 - metallicRoughness.x) * occlusion;
    surface.c_spec = mix(kDielectricSpecular, baseColor.rgb, metallicRoughness.x) * occlusion;
    surface.roughness = metallicRoughness.y;
    surface.alpha = surface.roughness * surface.roughness;
    surface.alphaSqr = surface.alpha * surface.alpha;

	// Begin accumulating light starting with emissive
    vec3 colorAccum = emissive;

	// Add IBL
    colorAccum += Diffuse_IBL(surface);
    colorAccum += Specular_IBL(surface);

	float sunShadow = texture(texSunShadow, vertOutput.sunShadowCoord).x;
	colorAccum += ShadeDirectionalLight(surface, SunDirection, sunShadow * SunIntensity);

	outColor = vec4(colorAccum, baseColor.a);
}