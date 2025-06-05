#include "Common.glsl"

layout (binding = kMeshConstants) uniform MeshConstants
{
	mat4 WorldMatrix;   // Object to world
	mat3 WorldIT;       // Object normal to world normal
};

layout (binding = kCommonConstants) uniform GlobalConstants
{
	mat4 ViewProjMatrix;
};

#ifdef ENABLE_SKINNING
struct Joint
{
    mat4 PosMatrix;
    mat3x4 NrmMatrix; // Inverse-transpose of PosMatrix
};

layout (binding = kSkinMatrices) buffer SkinMatrices
{
	Joint Joints[];
};
#endif

layout (location = 0) in vec3 pos;
#ifdef ENABLE_ALPHATEST
layout (location = 1) in vec2 uv0;
#endif
#ifdef ENABLE_SKINNING
layout (location = 2) in uvec4 jointIndices;
layout (location = 3) in vec4 jointWeights;
#endif

#ifdef ENABLE_ALPHATEST
layout (location = 0) out vec2 uv;
#endif

void main()
{
	vec4 position = vec4(pos, 1.0);

#ifdef ENABLE_SKINNING
    // I don't like this hack.  The weights should be normalized already, but something is fishy.
    vec4 weights = normalize(jointWeights);

    mat4 skinPosMat =
        Joints[jointIndices.x].PosMatrix * weights.x +
        Joints[jointIndices.y].PosMatrix * weights.y +
        Joints[jointIndices.z].PosMatrix * weights.z +
        Joints[jointIndices.w].PosMatrix * weights.w;

    position = skinPosMat * position;
#endif

	vec3 worldPos = (WorldMatrix * position).xyz;
	gl_Position = ViewProjMatrix * vec4(worldPos, 1.0);

#ifdef ENABLE_ALPHATEST
	uv = uv0;
#endif
}