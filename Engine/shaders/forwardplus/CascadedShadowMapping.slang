//
// Vertex
//

struct VertexInput
{
    float3 position;
    float3 normal;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 ndcPosition;
    float3 normal;
    float2 texCoord;
}

[[vk::binding(0, 1)]]
StructuredBuffer<float4x4> lightSpaceMatrices : register(t0);

struct InstanceTransformInfo
{
    float4x4 transform;
    float4x4 normal_matrix;
};

[[vk::binding(1, 0)]]
StructuredBuffer<InstanceTransformInfo> transformInfo : register(b1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<uint64_t> transformIndices : register(b2, space0);

struct PushConstant
{
    uint32_t numCascades;
    uint32_t numLights;
    uint64_t transformOffset;
}

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
    uint32_t meshInstanceIdx = instanceId / (pushConstant.numCascades * pushConstant.numLights);
    uint32_t lightCascadeIdx = instanceId % (pushConstant.numCascades * pushConstant.numLights);

    if (lightCascadeIdx >= lightSpaceMatrices.getCount())
    {
        return {};
    }

    uint32_t cascadeIdx = lightCascadeIdx % pushConstant.numCascades;
    uint32_t lightIdx = lightCascadeIdx / pushConstant.numCascades;

    uint64_t transformIndex = transformIndices[pushConstant.transformOffset + meshInstanceIdx];
    float4x4 transform = transformInfo[transformIndex].transform;

    float4x4 lightMatrix = lightSpaceMatrices[lightCascadeIdx];

    float4 pos = mul(lightMatrix, mul(transform, float4(input.position, 1.0f)));
    pos /= pos.w;

    float4 ndcPosition = pos;

    pos.x = transformToPosition(pos.x, cascadeIdx, pushConstant.numCascades);
    pos.y = transformToPosition(pos.y, lightIdx, pushConstant.numLights);

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
