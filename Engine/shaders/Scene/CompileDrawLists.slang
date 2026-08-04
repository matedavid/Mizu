#include "Base/Base.slang"
#include "Base/DescriptorBindings.slang"
#include "Base/GpuDrivenRendering.slang"

#ifndef GROUP_SIZE
#define GROUP_SIZE 16
#endif

struct GpuDrawableInstance
{
    float3 aabbMin;
    uint32_t transformSlot;
    float3 aabbMax;
    uint32_t materialOffset;

    uint32_t indexCount;
    uint32_t firstIndex;
    uint32_t firstVertex;
    uint32_t _pad;
};

struct GpuCullParams
{
    float4 planes[6];
    uint32_t frustumMask;

    uint32_t _pad[3];
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<GpuDrawableInstance> g_instances : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<InstanceTransformInfo> g_transformInfo : register(t1, space0);

MIZU_VK_BINDING_SRV(2, 0)
StructuredBuffer<GpuCullParams> g_cullParams : register(t2, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<uint32_t> g_visibleIndicesUav : register(u0, space0);

MIZU_VK_BINDING_UAV(1, 0)
RWStructuredBuffer<uint32_t> g_visibleCountUav : register(u1, space0);

struct CullingPushConstant
{
    uint32_t compileListIdx;
    uint32_t outputOffset;
};

[push_constant]
CullingPushConstant cullingPushConstant;

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void csCullInstances(uint3 threadId: SV_DispatchThreadID)
{
    uint32_t instanceId = threadId.x;
    if (instanceId >= g_instances.getCount())
        return;

    GpuDrawableInstance drawable = g_instances[instanceId];
    InstanceTransformInfo transformInfo = g_transformInfo[drawable.transformSlot];

    float3 center = (drawable.aabbMax + drawable.aabbMin) * 0.5f;
    float3 extent = (drawable.aabbMax - drawable.aabbMin) * 0.5f;

    float3 worldCenter = mul(transformInfo.transform, float4(center, 1.0f)).xyz;
    float3 worldExtent = float3(
        dot(abs(transformInfo.transform[0].xyz), extent),
        dot(abs(transformInfo.transform[1].xyz), extent),
        dot(abs(transformInfo.transform[2].xyz), extent));

    GpuCullParams cullParams = g_cullParams[cullingPushConstant.compileListIdx];
    for (uint i = 0; i < 6; ++i)
    {
        if ((cullParams.frustumMask & (1u << i)) == 0)
            continue;

        float4 plane = cullParams.planes[i];

        float s = dot(plane.xyz, worldCenter) + plane.w;
        float r = dot(abs(plane.xyz), worldExtent);
        if (s + r < 0.0)
            return;
    }

    uint32_t slot;
    InterlockedAdd(g_visibleCountUav[cullingPushConstant.compileListIdx], 1, slot);

    g_visibleIndicesUav[cullingPushConstant.outputOffset + slot] = instanceId;
}

MIZU_VK_BINDING_SRV(3, 0)
StructuredBuffer<uint32_t> g_visibleIndicesSrv : register(t3, space0);

MIZU_VK_BINDING_SRV(4, 0)
StructuredBuffer<uint32_t> g_visibleCountSrv : register(t4, space0);

MIZU_VK_BINDING_UAV(2, 0)
RWStructuredBuffer<DrawIndexedIndirectCommand> g_indirectCommands : register(u2, space0);

MIZU_VK_BINDING_UAV(3, 0)
RWStructuredBuffer<GpuDrawData> g_gpuDrawData : register(u3, space0);

struct GenerationPushConstant
{
    uint32_t indirectCommandsOffset;
    uint32_t visibleIndicesOffset;
    uint32_t compileListIdx;
    uint32_t viewCount;
};

[push_constant]
GenerationPushConstant generationPushConstant;

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void csGenerateCommands(uint3 threadId: SV_DispatchThreadID)
{
    uint32_t visibleCount = g_visibleCountSrv[generationPushConstant.compileListIdx];

    uint32_t instanceId = threadId.x;
    if (instanceId >= visibleCount)
        return;

    uint32_t visibleInstanceId = g_visibleIndicesSrv[generationPushConstant.visibleIndicesOffset + instanceId];
    GpuDrawableInstance drawable = g_instances[visibleInstanceId];

    uint32_t commandIdx = generationPushConstant.indirectCommandsOffset + instanceId;

    GpuDrawData gpuDrawData;
    gpuDrawData.transformSlot = drawable.transformSlot;
    gpuDrawData.materialOffset = drawable.materialOffset;

    g_gpuDrawData[commandIdx] = gpuDrawData;

    DrawIndexedIndirectCommand command;
    command.draw_index = instanceId;
    command.index_count = drawable.indexCount;
    command.instance_count = generationPushConstant.viewCount;
    command.first_index = drawable.firstIndex;
    command.vertex_offset = drawable.firstVertex;
    command.first_instance = 0;

    g_indirectCommands[commandIdx] = command;
}

