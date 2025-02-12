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
    pos = float4(pos.xyz / pos.w, 1.0f);

    {
        float step = 1.0f / float(lightViewMatrices.getCount());
        float min = float(instanceId) * step;

        float zeroOnePosX = (pos.x + 1.0f) / 2.0f;
        zeroOnePosX = zeroOnePosX * step + min;

        pos.x = 2.0f * zeroOnePosX - 1.0f;
    }

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
