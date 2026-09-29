#ifndef SCENE_RENDERER_VERTEX_COMMON
#define SCENE_RENDERER_VERTEX_COMMON

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
    float4 tangent : TEXCOORD1;
};

#endif