#include "base/base.slang"
#include "base/descriptor_bindings.slang"
#include "base/gpu_driven_rendering.slang"

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
    float4 ndc_position : POSITION1;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<InstanceTransformInfo> g_transform_info : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<GpuDrawData> g_draw_data : register(t1, space0);

MIZU_VK_BINDING_SRV(0, 1)
StructuredBuffer<float4x4> g_light_space_matrices : register(t0, space1);

struct CascadedShadowMappingInfo
{
    uint32_t num_cascades;
    uint32_t num_lights;
};

MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<CascadedShadowMappingInfo> g_cascaded_shadow_mapping_info : register(b0, space1);

struct PushConstant
{
    uint32_t draw_index;
};

[push_constant]
PushConstant push_constant;

float transform_to_position(float pos, uint32_t id, uint32_t num)
{
    float step_size = 1.0f / float(num);
    float min_value = float(id) * step_size;

    float zero_to_one_value = pos * 0.5f + 0.5f;
    zero_to_one_value = zero_to_one_value * step_size + min_value;

    return 2.0f * zero_to_one_value - 1.0f; // Convert back to [-1, 1]
}

[shader("vertex")]
VertexOutput vs_main(VertexInput input, uint32_t instance_id: SV_InstanceID MIZU_DRAW_INDEX_PARAM)
{
    uint32_t num_cascades = g_cascaded_shadow_mapping_info.num_cascades;
    uint32_t num_lights = g_cascaded_shadow_mapping_info.num_lights;

    uint32_t mesh_instance_idx = instance_id / (num_cascades * num_lights);
    uint32_t light_cascade_idx = instance_id % (num_cascades * num_lights);

    if (light_cascade_idx >= g_light_space_matrices.getCount())
    {
        return {};
    }

    uint32_t cascade_idx = light_cascade_idx % num_cascades;
    uint32_t light_idx = light_cascade_idx / num_cascades;

    uint32_t draw_data_idx = push_constant.draw_index + MIZU_GET_SV_DRAW_INDEX() + mesh_instance_idx;

    GpuDrawData draw_data = g_draw_data[draw_data_idx];
    float4x4 transform = g_transform_info[draw_data.transform_slot].transform;

    float4x4 light_matrix = g_light_space_matrices[light_cascade_idx];

    float4 pos = mul(light_matrix, mul(transform, float4(input.position, 1.0f)));
    pos /= pos.w;

    float4 ndc_position = pos;

    pos.x = transform_to_position(pos.x, cascade_idx, num_cascades);
    pos.y = transform_to_position(pos.y, light_idx, num_lights);

    VertexOutput output;
    output.position = pos;
    output.ndc_position = ndc_position;
    output.normal = input.normal;
    output.tex_coord = input.tex_coord;

    return output;
}

//
// Fragment
//

[shader("fragment")]
void fs_main(VertexOutput input)
{
    if (input.ndc_position.x < -1.0f || input.ndc_position.x > 1.0f || input.ndc_position.y < -1.0f
        || input.ndc_position.y > 1.0f)
    {
        discard;
    }
}
