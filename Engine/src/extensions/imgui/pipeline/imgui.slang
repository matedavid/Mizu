#include "base/base.slang"
#include "base/descriptor_bindings.slang"
#include "base/gpu_driven_rendering.slang"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint32_t color;

    uint32_t _padding;
};

struct ImGuiDrawData
{
    float2 scale;
    float2 translate;

    float2 clip_min;
    float2 clip_max;

    uint32_t vertex_offset;
    uint32_t index_offset;
    uint32_t bindless_slot;

    uint32_t _padding;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<ImGuiVertex> g_vertices : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<uint32_t> g_indices : register(t1, space0);

MIZU_VK_BINDING_SRV(2, 0)
StructuredBuffer<ImGuiDrawData> g_draw_data : register(t2, space0);

MIZU_VK_BINDING_SAMPLER(0, 0)
SamplerState g_sampler : register(s0, space0);

MIZU_VK_BINDING_SRV(0, 1)
Texture2D<float4> g_textures[] : register(t0, space1);

struct PushConstant
{
    uint32_t draw_data_idx;
};

[push_constant]
PushConstant push_constant;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation uint32_t bindless_slot : TEXCOORD1;

    float4 clip_distance : SV_ClipDistance0;
};

[shader("vertex")]
VSOutput vs_main(uint32_t vertex_id: SV_VertexID MIZU_DRAW_INDEX_PARAM)
{
    uint32_t draw_data_idx = push_constant.draw_data_idx + MIZU_GET_SV_DRAW_INDEX();
    ImGuiDrawData draw_data = g_draw_data[draw_data_idx];

    uint32_t index = g_indices[draw_data.index_offset + vertex_id];
    ImGuiVertex vertex = g_vertices[draw_data.vertex_offset + index];

    VSOutput output;
    output.position = float4(vertex.position * draw_data.scale + draw_data.translate, 0.0f, 1.0f);
    output.uv = vertex.uv;
    output.color = unpackUnorm4x8ToFloat(vertex.color);
    output.bindless_slot = draw_data.bindless_slot;
    output.clip_distance = float4(vertex.position - draw_data.clip_min, draw_data.clip_max - vertex.position);

    return output;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target
{
    return input.color * g_textures[NonUniformResourceIndex(input.bindless_slot)].Sample(g_sampler, input.uv);
}
