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
    uint32_t numLights;
    uint32_t numCascades;
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

    uint32_t cascadeIdx = instanceId / modelInfo.numLights;
    uint32_t lightIdx = instanceId % modelInfo.numLights;

    float4x4 lightViewMatrix = lightSpaceMatrices[instanceId];

    float4 pos = mul(lightViewMatrix, mul(modelInfo.model, float4(input.position, 1.0)));
    pos = float4(pos.xyz / pos.w, 1.0f);

    if (pos.x <= -1.0f || pos.x >= 1.0f || pos.y <= -1.0f || pos.y >= 1.0f || pos.z <= 0.0f || pos.z >= 1.0f)
    {
        // TODO: Sketchy
        // If NDC position is invalid, we don't want to render it but because later we clamp to [-1.0, 1.0] range in
        // order to move to correct shadowmap position, the position will become valid.
        // We can't just return the invalid position because it will be rendered to the complete shadowmap. Just return
        // a very big depth so it fails the depth test and therefore it doesen't get written to the depth texture.
        // I don't like this, this is sketchy

        VertexOutput output;
        output.position = float4(pos.x, pos.y, 100000.0f, 1.0f);

        return output;
    }

    // Give a bit of a border between shadow maps to prevent acccesing wrong shadow map on lighting pass
    const float BORDER = 0.001f;
    pos.x = clamp(pos.x, -1.0f + BORDER, 1.0f - BORDER);
    // pos.y = clamp(pos.y, -1.0f + BORDER, 1.0f - BORDER);

    {
        // TODO: Look into why it doesen't work

        float stepX = 1.0f / float(modelInfo.numLights);
        float minX = float(lightIdx) * stepX;

        float zeroOnePosX = pos.x * 0.5f + 0.5f;
        zeroOnePosX = zeroOnePosX * stepX + minX;

        pos.x = 2.0f * zeroOnePosX - 1.0f;
    }

    {
        float stepY = 1.0f / float(modelInfo.numCascades);
        float minY = float(cascadeIdx) * stepY;

        float zeroOnePosY = pos.y * 0.5f + 0.5f;
        zeroOnePosY = zeroOnePosY * stepY + minY;

        pos.y = 2.0f * zeroOnePosY - 1.0f;
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
