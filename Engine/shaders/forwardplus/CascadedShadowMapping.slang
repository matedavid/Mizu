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

struct PushConstant
{
    float4x4 model;
    uint32_t numCascades;
    uint32_t numLights;
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
    if (instanceId >= lightSpaceMatrices.getCount())
    {
        return {};
    }

    uint32_t cascadeIdx = instanceId % pushConstant.numCascades;
    uint32_t lightIdx = instanceId / pushConstant.numCascades;

    float4x4 lightMatrix = lightSpaceMatrices[instanceId];

    float4 pos = mul(lightMatrix, mul(pushConstant.model, float4(input.position, 1.0f)));
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
    if (input.ndcPosition.x < -1.0f || input.ndcPosition.x > 1.0f || input.ndcPosition.y < -1.0f || input.ndcPosition.y > 1.0f)
    {
        discard;
    }
}
