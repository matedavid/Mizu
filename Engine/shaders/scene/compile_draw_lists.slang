#include "base/base.slang"
#include "base/descriptor_bindings.slang"
#include "base/gpu_driven_rendering.slang"

#ifndef GROUP_SIZE
#define GROUP_SIZE 16
#endif

struct GpuDrawableInstance
{
    float3 aabb_min;
    uint32_t transform_slot;
    float3 aabb_max;
    uint32_t material_offset;

    uint32_t index_count;
    uint32_t first_index;
    uint32_t first_vertex;
    uint32_t _pad;
};

struct GpuCullParams
{
    float4 planes[6];
    uint32_t frustum_mask;

    uint32_t _pad[3];
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<GpuDrawableInstance> g_instances : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<InstanceTransformInfo> g_transform_info : register(t1, space0);

MIZU_VK_BINDING_SRV(2, 0)
StructuredBuffer<GpuCullParams> g_cull_params : register(t2, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<uint32_t> g_visible_indices_uav : register(u0, space0);

MIZU_VK_BINDING_UAV(1, 0)
RWStructuredBuffer<uint32_t> g_visible_count_uav : register(u1, space0);

struct CullingPushConstant
{
    uint32_t compile_list_idx;
    uint32_t output_offset;
};

[push_constant]
CullingPushConstant culling_push_constant;

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void cs_cull_instances(uint3 thread_id: SV_DispatchThreadID)
{
    uint32_t instance_id = thread_id.x;
    if (instance_id >= g_instances.getCount())
        return;

    GpuDrawableInstance drawable = g_instances[instance_id];
    InstanceTransformInfo transform_info = g_transform_info[drawable.transform_slot];

    float3 center = (drawable.aabb_max + drawable.aabb_min) * 0.5f;
    float3 extent = (drawable.aabb_max - drawable.aabb_min) * 0.5f;

    float3 world_center = mul(transform_info.transform, float4(center, 1.0f)).xyz;
    float3 world_extent = float3(
        dot(abs(transform_info.transform[0].xyz), extent),
        dot(abs(transform_info.transform[1].xyz), extent),
        dot(abs(transform_info.transform[2].xyz), extent));

    GpuCullParams cull_params = g_cull_params[culling_push_constant.compile_list_idx];
    for (uint i = 0; i < 6; ++i)
    {
        if ((cull_params.frustum_mask & (1u << i)) == 0)
            continue;

        float4 plane = cull_params.planes[i];

        float s = dot(plane.xyz, world_center) + plane.w;
        float r = dot(abs(plane.xyz), world_extent);
        if (s + r < 0.0)
            return;
    }

    uint32_t slot;
    InterlockedAdd(g_visible_count_uav[culling_push_constant.compile_list_idx], 1, slot);

    g_visible_indices_uav[culling_push_constant.output_offset + slot] = instance_id;
}

MIZU_VK_BINDING_SRV(3, 0)
StructuredBuffer<uint32_t> g_visible_indices_srv : register(t3, space0);

MIZU_VK_BINDING_SRV(4, 0)
StructuredBuffer<uint32_t> g_visible_count_srv : register(t4, space0);

MIZU_VK_BINDING_UAV(2, 0)
RWStructuredBuffer<DrawIndexedIndirectCommand> g_indirect_commands : register(u2, space0);

MIZU_VK_BINDING_UAV(3, 0)
RWStructuredBuffer<GpuDrawData> g_gpu_draw_data : register(u3, space0);

struct GenerationPushConstant
{
    uint32_t indirect_commands_offset;
    uint32_t visible_indices_offset;
    uint32_t compile_list_idx;
    uint32_t view_count;
};

[push_constant]
GenerationPushConstant generation_push_constant;

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void cs_generate_commands(uint3 thread_id: SV_DispatchThreadID)
{
    uint32_t visible_count = g_visible_count_srv[generation_push_constant.compile_list_idx];

    uint32_t instance_id = thread_id.x;
    if (instance_id >= visible_count)
        return;

    uint32_t visible_instance_id = g_visible_indices_srv[generation_push_constant.visible_indices_offset + instance_id];
    GpuDrawableInstance drawable = g_instances[visible_instance_id];

    uint32_t command_idx = generation_push_constant.indirect_commands_offset + instance_id;

    GpuDrawData gpu_draw_data;
    gpu_draw_data.transform_slot = drawable.transform_slot;
    gpu_draw_data.material_offset = drawable.material_offset;

    g_gpu_draw_data[command_idx] = gpu_draw_data;

    DrawIndexedIndirectCommand command;
    command.draw_index = instance_id;
    command.index_count = drawable.index_count;
    command.instance_count = generation_push_constant.view_count;
    command.first_index = drawable.first_index;
    command.vertex_offset = drawable.first_vertex;
    command.first_instance = 0;

    g_indirect_commands[command_idx] = command;
}
