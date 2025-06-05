#version 450

layout (push_constant) uniform FontParamsVert
{
    vec2 Scale;            // Scale and offset for transforming coordinates
    vec2 Offset;
    vec2 InvTexDim;        // Normalizes texture coordinates
    float TextSize;            // Height of text in destination pixels
    float TextScale;        // TextSize / FontHeight
    float DstBorder;        // Extra space around a glyph measured in screen space coordinates
    uint SrcBorder;            // Extra spacing around glyphs to avoid sampling neighboring glyphs
};

layout(location = 0) in vec2 ScreenPos;
layout(location = 1) in uvec4 Glyph;    // U, V, W, H

layout(location = 0) out vec2 Tex;

void main()
{
    vec2 xy0 = ScreenPos - DstBorder;
    vec2 xy1 = ScreenPos + DstBorder + vec2(TextScale * Glyph.z, TextSize);
    uvec2 uv0 = Glyph.xy - SrcBorder;
    uvec2 uv1 = Glyph.xy + SrcBorder + Glyph.zw;

    // get uv0=(0,0), uv1=(1,0), uv2=(0,1), uv3=(1,1)
    vec2 uv = vec2( gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1 );
    
    gl_Position = vec4( mix(xy0, xy1, uv) * Scale + Offset, 0, 1 );
    Tex = mix(uv0, uv1, uv) * InvTexDim;
}
