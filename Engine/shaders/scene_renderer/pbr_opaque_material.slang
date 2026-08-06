#include "base/base.slang"
#include "base/descriptor_bindings.slang"
#include "base/gpu_driven_rendering.slang"
#include "base/utils.slang"

//
// Vertex
//

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 world_position : POSITION1;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;

    nointerpolation uint32_t material_offset : TEXCOORD1;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<InstanceTransformInfo> g_transform_info : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<GpuDrawData> g_draw_data : register(t1, space0);

MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<CameraInfo> g_camera_info : register(b0, space1);

struct PushConstant
{
    uint32_t draw_index;
};

[push_constant]
PushConstant push_constant;

[shader("vertex")]
VertexOutput vs_main(VertexInput input, uint32_t instance_id: SV_InstanceID MIZU_DRAW_INDEX_PARAM)
{
    float4 pos = float4(input.position, 1.0f);

    uint32_t draw_data_index = push_constant.draw_index + MIZU_GET_SV_DRAW_INDEX() + instance_id;

    GpuDrawData draw_data = g_draw_data[draw_data_index];
    InstanceTransformInfo transform_info = g_transform_info[draw_data.transform_slot];

    float4x4 model_matrix = transform_info.transform;
    float3x3 normal_matrix = float3x3(transform_info.normal_matrix);

    VertexOutput output;
    output.position = mul(g_camera_info.view_proj, mul(model_matrix, pos));
    output.world_position = mul(model_matrix, pos);
    output.normal = normalize(mul(normal_matrix, input.normal));
    output.tex_coord = input.tex_coord;
    output.material_offset = draw_data.material_offset;

    return output;
}

//
// Fragment
//

MIZU_VK_BINDING_SRV(0, 1)
StructuredBuffer<uint32_t> g_material_buffer : register(t0, space1);

MIZU_VK_BINDING_SAMPLER(0, 1)
SamplerState g_sampler : register(s0, space1);

MIZU_VK_BINDING_SRV(0, 2)
Texture2D<float4> g_textures[] : register(t0, space2);

struct FragmentOutput
{
    float2 gbuffer0 : SV_Target0;
    float4 gbuffer1 : SV_Target1;
    float4 gbuffer2 : SV_Target2;
};

#define MIZU_TEXTURE_OFFSET_ALBEDO 0
#define MIZU_TEXTURE_OFFSET_METALLIC 1
#define MIZU_TEXTURE_OFFSET_ROUGHNESS 2
#define MIZU_TEXTURE_OFFSET_AO 3

Texture2D<float4> get_texture(uint32_t offset, uint32_t texture)
{
    uint32_t texture_index = g_material_buffer[offset + texture];
    return g_textures[texture_index];
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input) : SV_Target0
{
    Texture2D<float4> albedo_texture = get_texture(input.material_offset, MIZU_TEXTURE_OFFSET_ALBEDO);
    Texture2D<float4> metallic_texture = get_texture(input.material_offset, MIZU_TEXTURE_OFFSET_METALLIC);
    Texture2D<float4> roughness_texture = get_texture(input.material_offset, MIZU_TEXTURE_OFFSET_ROUGHNESS);
    Texture2D<float4> ao_texture = get_texture(input.material_offset, MIZU_TEXTURE_OFFSET_AO);

    float4 albedo = albedo_texture.Sample(g_sampler, input.tex_coord);
    float metallic = metallic_texture.Sample(g_sampler, input.tex_coord).r;
    float roughness = roughness_texture.Sample(g_sampler, input.tex_coord).g;
    float ao = ao_texture.Sample(g_sampler, input.tex_coord).b;

    FragmentOutput output;
    output.gbuffer0 = encode_normal_spherical(input.normal);
    output.gbuffer1 = albedo;
    output.gbuffer2 = float4(metallic, roughness, ao, 0.0f);

    return output;
}
