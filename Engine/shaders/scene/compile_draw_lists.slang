#include "base/base.slang"
#include "base/descriptor_bindings.slang"
#include "base/gpu_driven_rendering.slang"

#ifndef GROUP_SIZE
#define GROUP_SIZE 64
#endif

static const uint32_t INVALID_BUCKET_SLOT = 0xFFFFFFFF;

struct GpuDrawableInstance
{
    float3 aabb_min;
    uint32_t transform_slot;
    float3 aabb_max;
    uint32_t material_offset;

    uint32_t index_count;
    uint32_t first_index;
    uint32_t first_vertex;
    // Low 16 bits are the draw class id, high 16 bits are the drawable flags.
    uint32_t class_and_flags;
};

struct GpuVisibilityInfo
{
    float4 planes[6];
    uint32_t frustum_mask;
    uint32_t draw_list_begin;
    uint32_t draw_list_count;
    uint32_t _pad;
};

struct GpuDrawListInfo
{
    uint32_t bucket_map_offset;
    uint32_t view_count;
    uint32_t filter_required;
    uint32_t filter_excluded;
};

struct GpuBucketInfo
{
    uint32_t region_base;
    uint32_t capacity;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<GpuDrawableInstance> g_instances : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<InstanceTransformInfo> g_transform_info : register(t1, space0);

MIZU_VK_BINDING_SRV(2, 0)
StructuredBuffer<GpuVisibilityInfo> g_visibilities : register(t2, space0);

MIZU_VK_BINDING_SRV(3, 0)
StructuredBuffer<uint32_t> g_visibility_lists : register(t3, space0);

MIZU_VK_BINDING_SRV(4, 0)
StructuredBuffer<GpuDrawListInfo> g_draw_lists : register(t4, space0);

MIZU_VK_BINDING_SRV(5, 0)
StructuredBuffer<uint32_t> g_bucket_map : register(t5, space0);

MIZU_VK_BINDING_SRV(6, 0)
StructuredBuffer<GpuBucketInfo> g_buckets : register(t6, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<uint32_t> g_counts : register(u0, space0);

MIZU_VK_BINDING_UAV(1, 0)
RWStructuredBuffer<DrawIndexedIndirectCommand> g_commands : register(u1, space0);

MIZU_VK_BINDING_UAV(2, 0)
RWStructuredBuffer<GpuDrawData> g_draw_data : register(u2, space0);

struct CullAndGeneratePushConstant
{
    uint32_t visibility_idx;
    uint32_t num_drawables;
};

[push_constant]
CullAndGeneratePushConstant push_constant;

bool is_visible(GpuDrawableInstance drawable, GpuVisibilityInfo visibility)
{
    if (visibility.frustum_mask == 0)
        return true;

    InstanceTransformInfo transform_info = g_transform_info[drawable.transform_slot];

    float3 center = (drawable.aabb_max + drawable.aabb_min) * 0.5f;
    float3 extent = (drawable.aabb_max - drawable.aabb_min) * 0.5f;

    float3 world_center = mul(transform_info.transform, float4(center, 1.0f)).xyz;
    float3 world_extent = float3(
        dot(abs(transform_info.transform[0].xyz), extent),
        dot(abs(transform_info.transform[1].xyz), extent),
        dot(abs(transform_info.transform[2].xyz), extent));

    for (uint32_t i = 0; i < 6; ++i)
    {
        if ((visibility.frustum_mask & (1u << i)) == 0)
            continue;

        float4 plane = visibility.planes[i];

        float s = dot(plane.xyz, world_center) + plane.w;
        float r = dot(abs(plane.xyz), world_extent);
        if (s + r < 0.0)
            return false;
    }

    return true;
}

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void cs_cull_and_generate(uint3 thread_id: SV_DispatchThreadID)
{
    uint32_t instance_id = thread_id.x;
    if (instance_id >= push_constant.num_drawables)
        return;

    GpuDrawableInstance drawable = g_instances[instance_id];
    GpuVisibilityInfo visibility = g_visibilities[push_constant.visibility_idx];

    if (!is_visible(drawable, visibility))
        return;

    uint32_t class_id = drawable.class_and_flags & 0xFFFF;
    uint32_t flags = drawable.class_and_flags >> 16;

    for (uint32_t i = 0; i < visibility.draw_list_count; ++i)
    {
        uint32_t draw_list_idx = g_visibility_lists[visibility.draw_list_begin + i];
        GpuDrawListInfo draw_list = g_draw_lists[draw_list_idx];

        if ((flags & draw_list.filter_required) != draw_list.filter_required)
            continue;

        if ((flags & draw_list.filter_excluded) != 0)
            continue;

        uint32_t slot = g_bucket_map[draw_list.bucket_map_offset + class_id];
        if (slot == INVALID_BUCKET_SLOT)
            continue;

        GpuBucketInfo bucket = g_buckets[slot];

        uint32_t local_index;
        InterlockedAdd(g_counts[slot], 1, local_index);

        if (local_index >= bucket.capacity)
            continue;

        uint32_t command_idx = bucket.region_base + local_index;

        GpuDrawData draw_data;
        draw_data.transform_slot = drawable.transform_slot;
        draw_data.material_offset = drawable.material_offset;

        g_draw_data[command_idx] = draw_data;

        DrawIndexedIndirectCommand command;
        // Relative to the bucket region, the base is added through the push constant at draw time.
        command.draw_index = local_index;
        command.index_count = drawable.index_count;
        command.instance_count = draw_list.view_count;
        command.first_index = drawable.first_index;
        command.vertex_offset = drawable.first_vertex;
        command.first_instance = 0;

        g_commands[command_idx] = command;
    }
}
