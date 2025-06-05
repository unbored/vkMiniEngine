#version 450

#extension GL_GOOGLE_include_directive : enable
#include "Common.glsl"

layout(binding = kMeshConstants) uniform VertConstants
{
	mat4 ProjInverse;
	mat3 ViewInverse;
};

layout(location = 0) out vec3 viewDir;

void main()
{
	// Texture coordinates range [0, 2], but only [0, 1] appears on screen.
    // uvec0=(0,0), uvec1=(1,2), uvec2=(2,4)
    // vec0=(0,0), vec1=(0,2), vec2=(2,0)
    vec2 screenUV = vec2(uvec2(gl_VertexIndex, gl_VertexIndex << 1) & uvec2(2));
	// pos0=(-1,1), pos1=(-1,-3), pos2=(3,1)
    vec4 ProjectedPos = vec4(mix(vec2(-1, 1), vec2(1, -1), screenUV), 0, 1);
	vec4 PosViewSpace = ProjInverse * ProjectedPos;

	gl_Position = ProjectedPos;
	// divide w makes no sense here
	viewDir = ViewInverse * (PosViewSpace.xyz /* / PosViewSpace.w */ );
}