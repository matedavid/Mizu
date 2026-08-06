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
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
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
    output.normal = normalize(mul(normal_matrix, input.normal));
    output.tex_coord = input.tex_coord;

    return output;
}

//
// Fragment
//

[shader("fragment")]
void fs_main(VertexOutput input) {}
