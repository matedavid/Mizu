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

    float4x4 lightViewMatrix = lightSpaceMatrices[instanceId];

    float4 pos = mul(lightViewMatrix, mul(pushConstant.model, float4(input.position, 1.0f)));

    uint32_t cascadeId = instanceId % pushConstant.numCascades;
    uint32_t lightId = instanceId / pushConstant.numCascades;

    pos.x = transformToPosition(pos.x, cascadeId, pushConstant.numCascades);
    pos.y = transformToPosition(pos.y, lightId, pushConstant.numLights);

    VertexOutput output;
    output.position = pos;
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

[shader("fragment")]
void fsMain(VertexOutput input) {}
