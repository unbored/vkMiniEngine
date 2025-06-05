#include "Common.glsl"

layout (binding = kMeshConstants) uniform MeshConstants
{
	mat4 WorldMatrix;   // Object to world
	mat3 WorldIT;       // Object normal to world normal
};

layout (binding = kCommonConstants) uniform GlobalConstants
{
	mat4 ViewProjMatrix;
    mat4 SunShadowMatrix;
    vec3 ViewerPos;
    vec3 SunDirection;
    vec3 SunIntensity;
};

#ifdef ENABLE_SKINNING
struct Joint
{
    mat4 PosMatrix;
    mat3x4 NrmMatrix; // Inverse-transpose of PosMatrix
};

readonly layout (binding = kSkinMatrices) buffer SkinMatrices
{
	Joint Joints[];
};
#endif

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
#ifndef NO_TANGENT_FRAME
layout (location = 2) in vec4 tang;
#endif
layout (location = 3) in vec2 uv0;
#ifndef NO_SECOND_UV
layout (location = 4) in vec2 uv1;
#endif
#ifdef ENABLE_SKINNING
layout (location = 5) in uvec4 jointIndices;
layout (location = 6) in vec4 jointWeights;
#endif

layout (location = 0) out VertOutput
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

void main()
{
	vec4 position = vec4(pos, 1.0);
	vec3 normal = norm;
#ifndef NO_TANGENT_FRAME
	vec4 tangent = tang;
#endif

#ifdef ENABLE_SKINNING
    // I don't like this hack.  The weights should be normalized already, but something is fishy.
    vec4 weights = jointWeights / dot(jointWeights, vec4(1));

    mat4 skinPosMat =
        Joints[jointIndices.x].PosMatrix * weights.x +
        Joints[jointIndices.y].PosMatrix * weights.y +
        Joints[jointIndices.z].PosMatrix * weights.z +
        Joints[jointIndices.w].PosMatrix * weights.w;

    position = skinPosMat * position;

	mat3x4 skinNrmMat = 
	    Joints[jointIndices.x].NrmMatrix * weights.x +
        Joints[jointIndices.y].NrmMatrix * weights.y +
        Joints[jointIndices.z].NrmMatrix * weights.z +
        Joints[jointIndices.w].NrmMatrix * weights.w;

	normal = (skinNrmMat * normal).xyz;
#ifndef NO_TANGENT_FRAME
	tangent.xyz = (skinNrmMat * tangent.xyz).xyz;
#endif 

#endif

	vertOutput.worldPos = (WorldMatrix * position).xyz;
	gl_Position = ViewProjMatrix * vec4(vertOutput.worldPos, 1.0);
	vertOutput.sunShadowCoord = (SunShadowMatrix * vec4(vertOutput.worldPos, 1.0)).xyz;
	vertOutput.normal = WorldIT * normal;
#ifndef NO_TANGENT_FRAME
	vertOutput.tangent = vec4((WorldIT * tangent.xyz), tangent.w);
#endif 
	vertOutput.uv0 = uv0;
#ifndef NO_SECOND_UV
    vertOutput.uv1 = uv1;
#endif
}