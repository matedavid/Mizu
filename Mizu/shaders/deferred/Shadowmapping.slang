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

struct ModelInfo
{
    float4x4 model;
}

[push_constant]
ModelInfo uModelInfo;

[[vk::binding(0, 1)]]
StructuredBuffer<float4x4> lightViewMatrices : register(t0, space1);

[shader("vertex")]
VertexOutput vsMain(VertexInput input, uint32_t instanceId: SV_InstanceID)
{
    if (instanceId >= lightViewMatrices.getCount())
    {
        return {};
    }

    float4x4 lightViewMatrix = lightViewMatrices[instanceId];

    float4 pos = mul(lightViewMatrix, mul(uModelInfo.model, float4(input.position, 1.0)));
    pos.x *= float(instanceId + 1) / float(lightViewMatrices.getCount());

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
