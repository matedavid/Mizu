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
ModelInfo modelInfo;

[[vk::binding(0, 1)]]
StructuredBuffer<float4x4> lightSpaceMatrices : register(t0, space1);

[shader("vertex")]
VertexOutput vsMain(VertexInput input, uint32_t instanceId: SV_InstanceID)
{
    if (instanceId >= lightSpaceMatrices.getCount())
    {
        return {};
    }

    float4x4 lightViewMatrix = lightSpaceMatrices[instanceId];

    float4 pos = mul(lightViewMatrix, mul(modelInfo.model, float4(input.position, 1.0)));
    pos = float4(pos.xyz / pos.w, 1.0f);

    // Give a bit of a border between shadow maps to prevent acccesing wrong shadow map on lighting pass
    const float BORDER = 0.001f;
    pos.x = clamp(pos.x, -1.0f + BORDER, 1.0f - BORDER);

    /*
    if (pos.x < -1.0f || pos.x > 1.0 || pos.y < -1.0f || pos.y > 1.0f)
    {
        // Dummy position outside of the NDC range
        VertexOutput output;
        output.position = float4(-1.5f, -1.5f, 0.0f, 1.0f);

        return output;
    }
    */

    {
        float step = 1.0f / float(lightSpaceMatrices.getCount());
        float min = float(instanceId) * step;

        float zeroOnePosX = pos.x * 0.5f + 0.5f;
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
