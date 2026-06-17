#include "common/MizuBase.slang"

#ifndef GROUP_SIZE
#define GROUP_SIZE 16
#endif

struct TransformInfo
{
    float4x4 transformMatrix;
    float4x4 worldMatrix;
};

struct PendingTransformUpdate
{
    TransformInfo transformInfo;
    uint32_t dstSlot;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<PendingTransformUpdate> pendingUpdates : register(t0, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<TransformInfo> transformBuffer : register(u0, space0);

struct PushConstant
{
    uint64_t updateCount;
};

[push_constant]
PushConstant pushConstant;

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void csMain(uint3 threadId: SV_DispatchThreadID) 
{
    uint64_t threadIdx = threadId.x;
    if (threadIdx >= pushConstant.updateCount)
        return;

    PendingTransformUpdate pendingInfo = pendingUpdates[threadIdx];
    transformBuffer[pendingInfo.dstSlot] = pendingInfo.transformInfo;
}
