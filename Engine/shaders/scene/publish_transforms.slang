#include "base/descriptor_bindings.slang"

#ifndef GROUP_SIZE
#define GROUP_SIZE 16
#endif

struct TransformInfo
{
    float4x4 transform;
    float4x4 normal_matrix;
};

struct PendingTransformUpdate
{
    TransformInfo transform_info;
    uint32_t dst_slot;

    uint32_t _padding[3];
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<PendingTransformUpdate> g_pending_updates : register(t0, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<TransformInfo> g_transform_buffer : register(u0, space0);

struct PushConstant
{
    uint64_t update_count;
};

[push_constant]
PushConstant push_constant;

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void cs_main(uint3 thread_id: SV_DispatchThreadID)
{
    uint64_t thread_idx = thread_id.x;
    if (thread_idx >= push_constant.update_count)
        return;

    PendingTransformUpdate pending_info = g_pending_updates[thread_idx];
    g_transform_buffer[pending_info.dst_slot] = pending_info.transform_info;
}
