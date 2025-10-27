#include "common/FrameInfo.slang"
#include "common/Lights.slang"
#include "common/MizuBase.slang"
#include "forwardplus/LightCullingCommon.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

[[vk::binding(0, 1)]]
StructuredBuffer<PointLight> pointLights : register(t0, space1);
[[vk::binding(1, 1)]]
RWStructuredBuffer<uint32_t> visiblePointLightIndices : register(u0, space1);
[[vk::binding(2, 1)]]
ConstantBuffer<LightCullingInfo> lightCullingInfo : register(b0, space1);

[[vk::binding(0, 2)]]
Texture2D<float> depthTexture : register(t0, space2);
[[vk::binding(1, 2)]]
SamplerState depthTextureSampler : register(s0, space2);

groupshared uint32_t tileMinDepth;
groupshared uint32_t tileMaxDepth;
groupshared uint32_t tileVisibleLightCount;
groupshared float4 tileFrustumPlanes[6];
groupshared uint32_t tileVisibleLightIndices[MAX_LIGHTS_PER_TILE];

float linearizeDepth(float depth, float znear, float zfar)
{
    return znear * zfar / (zfar + depth * (znear - zfar));
}

[shader("compute")]
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void csMain(uint32_t groupIndex: SV_GroupIndex, uint3 groupId: SV_GroupID, uint3 threadId: SV_DispatchThreadID)
{
    // Compute values
    uint32_t width, height;
    depthTexture.GetDimensions(width, height);

    uint32_t tileIndex = groupId.y * lightCullingInfo.numTiles.x + groupId.x;

    // 1. Initialize per-tile information

    if (groupIndex == 0)
    {
        tileMinDepth = 0xFFFFFFFF;
        tileMaxDepth = 0;
        tileVisibleLightCount = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    // 2. Calculate tile min and max depth values

    float2 texCoords = float2(threadId.xy) / float2(width, height);
    float depth = depthTexture.SampleLevel(depthTextureSampler, texCoords, 0);

    float linearDepth = linearizeDepth(depth, cameraInfo.znear, cameraInfo.zfar);

    uint32_t uintLinearDepth = asuint(linearDepth);
    InterlockedMin(tileMinDepth, uintLinearDepth);
    InterlockedMax(tileMaxDepth, uintLinearDepth);

    GroupMemoryBarrierWithGroupSync();

    // 3. Calculate tile frustums

    if (groupIndex == 0)
    {
        float minDepth = asfloat(tileMinDepth) - 0.01f;
        float maxDepth = asfloat(tileMaxDepth) + 0.01f;

        float2 negativeStep = (2.0f * float2(groupId.xy)) / float2(lightCullingInfo.numTiles);
        float2 positiveStep = (2.0f * float2(groupId.xy + uint2(1))) / float2(lightCullingInfo.numTiles);

        // clang-format off
        tileFrustumPlanes[0] = float4(1.0f,  0.0f,  0.0f,  1.0f - negativeStep.x); // Left
        tileFrustumPlanes[1] = float4(-1.0f, 0.0f,  0.0f, -1.0f + positiveStep.x); // Right
        tileFrustumPlanes[2] = float4(0.0f,  1.0f,  0.0f,  1.0f - negativeStep.y); // Bottom
        tileFrustumPlanes[3] = float4(0.0f, -1.0f,  0.0f, -1.0f + positiveStep.y); // Top
        tileFrustumPlanes[4] = float4(0.0f,  0.0f, -1.0f, -minDepth);              // Near
        tileFrustumPlanes[5] = float4(0.0f,  0.0f,  1.0f,  maxDepth);              // Far
        // clang-format on

        // Transform the first four planes
        for (uint32_t i = 0; i < 4; ++i)
        {
            tileFrustumPlanes[i] = mul(tileFrustumPlanes[i], cameraInfo.viewProj);
            tileFrustumPlanes[i] /= length(tileFrustumPlanes[i].xyz);
        }

        // Transform the depth planes
        tileFrustumPlanes[4] = mul(tileFrustumPlanes[4], cameraInfo.view);
        tileFrustumPlanes[4] /= length(tileFrustumPlanes[4].xyz);
        tileFrustumPlanes[5] = mul(tileFrustumPlanes[5], cameraInfo.view);
        tileFrustumPlanes[5] /= length(tileFrustumPlanes[5].xyz);
    }

    GroupMemoryBarrierWithGroupSync();

    // 4. Cull lights

    uint32_t numPointLights = pointLights.getCount();

    uint32_t threadCount = TILE_SIZE * TILE_SIZE;
    uint32_t iterationsPerThread = (numPointLights + threadCount - 1) / threadCount;

    for (uint32_t i = 0; i < iterationsPerThread; ++i)
    {
        uint32_t lightIndex = i * threadCount + groupIndex;
        if (lightIndex >= numPointLights)
            break;

        PointLight light = pointLights[lightIndex];

        float distance = 0.0f;
        for (uint32_t j = 0; j < 6; ++j)
        {
            distance = dot(float4(light.position, 1.0f), tileFrustumPlanes[j]) + light.radius;
            if (distance <= 0.0f)
                break;
        }

        if (distance > 0.0f)
        {
            uint32_t originalValue;
            InterlockedAdd(tileVisibleLightCount, 1, originalValue);
            tileVisibleLightIndices[originalValue] = lightIndex;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // 5. Copy the tile visible lights into the global array

    if (groupIndex == 0)
    {
        uint32_t index = groupId.y * uint32_t(lightCullingInfo.numTiles.x) + groupId.x;

        uint32_t offset = index * MAX_LIGHTS_PER_TILE;
        for (uint32_t i = 0; i < tileVisibleLightCount; ++i)
        {
            visiblePointLightIndices[offset + i] = tileVisibleLightIndices[i];
        }

        if (tileVisibleLightCount != MAX_LIGHTS_PER_TILE)
        {
            visiblePointLightIndices[offset + tileVisibleLightCount] = 0xFFFFFFFF;
        }
    }
}
