
[[vk::binding(0, 2)]]
StructuredBuffer<uint32_t> visiblePointLightIndices : register(t0, space2);
[[vk::binding(1, 2)]]
RWTexture2D<float4> output : register(u0, space2);

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 128

[shader("compute")]
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void csMain(uint3 groupId: SV_GroupID, uint3 threadId: SV_DispatchThreadID)
{
    uint32_t width, height;
    output.GetDimensions(width, height);

    float3 numWorkGroups =
        float3(float(width + TILE_SIZE - 1) / TILE_SIZE, float(height + TILE_SIZE - 1) / TILE_SIZE, 1.0f);
    uint32_t index = groupId.y * uint32_t(numWorkGroups.x) + groupId.x;

    uint32_t offset = index * MAX_LIGHTS_PER_TILE;

    uint32_t numLights = 0;
    for (uint32_t i = 0; i < MAX_LIGHTS_PER_TILE; ++i)
    {
        if (visiblePointLightIndices[offset + i] == 0xFFFFFFFF)
            break;

        numLights += 1;
    }

    float c = float(numLights) / float(MAX_LIGHTS_PER_TILE);
    output[threadId.xy] = float4(c, c, c, 1.0f);
}
