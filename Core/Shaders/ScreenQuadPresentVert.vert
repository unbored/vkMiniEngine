#version 450

layout(location = 0) out vec2 Tex;

void main()
{
    // Texture coordinates range [0, 2], but only [0, 1] appears on screen.
    // uvec0=(0,0), uvec1=(1,2), uvec3=(2,4)
    // vec0=(0,0), vec1=(0,2), vec2=(2,0)
    Tex = vec2(uvec2(gl_VertexIndex, gl_VertexIndex << 1) & uvec2(2));
    // pos0=(-1,1), pos1=(-1,-3), pos2=(3,1)
    gl_Position = vec4(mix(vec2(-1, 1), vec2(1, -1), Tex), 0, 1);
}
