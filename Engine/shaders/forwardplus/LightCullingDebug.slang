//
// Vertex
//

struct VertexInput
{
    float3 position;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoord;
}

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

#include "forwardplus/LightCullingCommon.slang"

[[vk::binding(0, 0)]]
StructuredBuffer<uint32_t> visiblePointLightIndices : register(t0, space0);
[[vk::binding(1, 0)]]
ConstantBuffer<LightCullingInfo> lightCullingInfo : register(b0, space0);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    // SV_Position in hlsl fragment shader, represents the position in screen space
    float2 tileId = input.position.xy / float2(TILE_SIZE);
    uint32_t tileIndex = uint32_t(tileId.y) * lightCullingInfo.numTiles.x + uint32_t(tileId.x);
    uint32_t index = tileIndex * MAX_LIGHTS_PER_TILE;

    uint32_t numLights = 0;
    for (uint32_t i = 0; i < MAX_LIGHTS_PER_TILE; ++i)
    {
        uint32_t lightIndex = visiblePointLightIndices[index + i];
        if (lightIndex == 0xFFFFFFFF)
            break;

        numLights++;
    }

    float c = float(numLights) / float(20);
    return float4(c, 0.0f, 0.0f, 0.7f);
}
