#include "common/MizuBase.slang"

//
// Vertex
//

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 ndcPosition : POSITION1;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct InstanceTransformInfo
{
    float4x4 transform;
    float4x4 normal_matrix;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<InstanceTransformInfo> g_transformInfo : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<uint32_t> g_viewIndices : register(t1, space0);

MIZU_VK_BINDING_SRV(0, 1)
StructuredBuffer<float4x4> g_lightSpaceMatrices : register(t0, space1);

struct CascadedShadowMappingInfo
{ 
    uint32_t num_cascades;
    uint32_t num_lights;
};

MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<CascadedShadowMappingInfo> g_cascadedShadowMappingInfo : register(b0, space1);

//struct PushConstant
//{
//    uint32_t numCascades;
//    uint32_t numLights;
//    uint64_t transformOffset;
//};

struct PushConstant
{
    uint32_t viewIndicesOffset;
};


[push_constant]
PushConstant pushConstant;

float transformToPosition(float pos, uint32_t id, uint32_t num)
{
    float step = 1.0f / float(num);
    float min = float(id) * step;

    float zeroToOneValue = pos * 0.5f + 0.5f;
    zeroToOneValue = zeroToOneValue * step + min;

    return 2.0f * zeroToOneValue - 1.0f; // Convert back to [-1, 1]
}

[shader("vertex")]
VertexOutput vsMain(VertexInput input, uint32_t instanceId: SV_InstanceID)
{
    uint32_t numCascades = g_cascadedShadowMappingInfo.num_cascades;
    uint32_t numLights = g_cascadedShadowMappingInfo.num_lights;

    uint32_t meshInstanceIdx = instanceId / (numCascades * numLights);
    uint32_t lightCascadeIdx = instanceId % (numCascades * numLights);

    if (lightCascadeIdx >= g_lightSpaceMatrices.getCount())
    {
        return {};
    }

    uint32_t cascadeIdx = lightCascadeIdx % numCascades;
    uint32_t lightIdx = lightCascadeIdx / numCascades;

    uint64_t transformIndex = g_viewIndices[pushConstant.viewIndicesOffset + meshInstanceIdx];
    float4x4 transform = g_transformInfo[transformIndex].transform;

    float4x4 lightMatrix = g_lightSpaceMatrices[lightCascadeIdx];

    float4 pos = mul(lightMatrix, mul(transform, float4(input.position, 1.0f)));
    pos /= pos.w;

    float4 ndcPosition = pos;

    pos.x = transformToPosition(pos.x, cascadeIdx, numCascades);
    pos.y = transformToPosition(pos.y, lightIdx, numLights);

    // On rasterizers where NDC y=+1 maps to the top of the screen (low UV-V),
    // flip Y so that depth values are stored at the UV coordinates that the
    // lighting pass expects when it reconstructs UVs as (ndc.y * 0.5 + 0.5).
#ifdef MIZU_NDC_Y_UP
    pos.y = -pos.y;
#endif

    VertexOutput output;
    output.position = pos;
    output.ndcPosition = ndcPosition;
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

[shader("fragment")]
void fsMain(VertexOutput input)
{
    if (input.ndcPosition.x < -1.0f || input.ndcPosition.x > 1.0f || input.ndcPosition.y < -1.0f
        || input.ndcPosition.y > 1.0f)
    {
        discard;
    }
}
