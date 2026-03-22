#include "common/FrameInfo.slang"
#include "common/Lights.slang"
#include "common/MizuBase.slang"
#include "forwardplus/LightCullingCommon.slang"

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

MIZU_VK_BINDING_SRV(0, 1)
StructuredBuffer<PointLight> pointLights : register(t0, space1);
MIZU_VK_BINDING_UAV(0, 1)
RWStructuredBuffer<uint32_t> visiblePointLightIndices : register(u0, space1);
MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<LightCullingInfo> lightCullingInfo : register(b0, space1);

MIZU_VK_BINDING_SRV(0, 2)
Texture2D<float> depthTexture : register(t0, space2);
MIZU_VK_BINDING_SAMPLER(0, 2)
SamplerState depthTextureSampler : register(s0, space2);

groupshared uint32_t tileMinDepth;
groupshared uint32_t tileMaxDepth;
groupshared uint32_t tileVisibleLightCount;
groupshared float4 tileFrustumPlanes[6];
groupshared uint32_t tileVisibleLightIndices[MAX_LIGHTS_PER_TILE];

float4 createPlaneFromPoints(float3 point0, float3 point1, float3 point2, float3 insidePoint)
{
    float3 normal = normalize(cross(point1 - point0, point2 - point0));
    float4 plane = float4(normal, -dot(normal, point0));

    if (dot(float4(insidePoint, 1.0f), plane) < 0.0f)
    {
        plane *= -1.0f;
    }

    return plane;
}

float linearizeDepth(float depth, float znear, float zfar)
{
    return znear * zfar / (zfar + depth * (znear - zfar));
}

float2 getProjectionUVFromScreenUV(float2 screenUV, float4x4 projection)
{
    // Tile coordinates are generated in screen space, but the inverse projection expects the
    // same Y convention that was used to build the projection matrix. DX12 keeps the projection
    // unflipped while Vulkan flips it on the CPU, so we derive the UV remap from proj[1][1]
    // to keep the reconstructed tile frustum aligned with the actual screen tiles on both APIs.
    if (projection[1][1] > 0.0f)
    {
        return float2(screenUV.x, 1.0f - screenUV.y);
    }

    return screenUV;
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

    if (threadId.x < width && threadId.y < height)
    {
        float2 texCoords = float2(threadId.xy) / float2(width, height);
        float depth = depthTexture.SampleLevel(depthTextureSampler, texCoords, 0);

        float linearDepth = linearizeDepth(depth, cameraInfo.znear, cameraInfo.zfar);

        uint32_t uintLinearDepth = asuint(linearDepth);
        InterlockedMin(tileMinDepth, uintLinearDepth);
        InterlockedMax(tileMaxDepth, uintLinearDepth);
    }

    GroupMemoryBarrierWithGroupSync();

    // 3. Calculate tile frustums

    if (groupIndex == 0)
    {
        float minDepth = asfloat(tileMinDepth) - 0.01f;
        float maxDepth = asfloat(tileMaxDepth) + 0.01f;

        uint2 tileMinPixel = groupId.xy * TILE_SIZE;
        uint2 tileMaxPixel = min(tileMinPixel + uint2(TILE_SIZE), uint2(width, height));

        float2 tileMinUV = float2(tileMinPixel) / float2(width, height);
        float2 tileMaxUV = float2(tileMaxPixel) / float2(width, height);
        float2 tileCenterUV = 0.5f * (tileMinUV + tileMaxUV);
        float2 tileMinProjectionUV = getProjectionUVFromScreenUV(tileMinUV, cameraInfo.proj);
        float2 tileMaxProjectionUV = getProjectionUVFromScreenUV(tileMaxUV, cameraInfo.proj);
        float2 tileCenterProjectionUV = getProjectionUVFromScreenUV(tileCenterUV, cameraInfo.proj);

        float3 topLeft = viewSpacePositionFromDepth(float2(tileMinProjectionUV.x, tileMaxProjectionUV.y), 1.0f, cameraInfo.inverseProj);
        float3 topRight = viewSpacePositionFromDepth(float2(tileMaxProjectionUV.x, tileMaxProjectionUV.y), 1.0f, cameraInfo.inverseProj);
        float3 bottomLeft = viewSpacePositionFromDepth(float2(tileMinProjectionUV.x, tileMinProjectionUV.y), 1.0f, cameraInfo.inverseProj);
        float3 bottomRight = viewSpacePositionFromDepth(float2(tileMaxProjectionUV.x, tileMinProjectionUV.y), 1.0f, cameraInfo.inverseProj);
        float3 insidePoint = viewSpacePositionFromDepth(tileCenterProjectionUV, 1.0f, cameraInfo.inverseProj);
        float3 origin = float3(0.0f, 0.0f, 0.0f);

        tileFrustumPlanes[0] = createPlaneFromPoints(origin, bottomLeft,  topLeft,     insidePoint); // Left
        tileFrustumPlanes[1] = createPlaneFromPoints(origin, topRight,    bottomRight, insidePoint); // Right
        tileFrustumPlanes[2] = createPlaneFromPoints(origin, topLeft,     topRight,    insidePoint); // Top
        tileFrustumPlanes[3] = createPlaneFromPoints(origin, bottomRight, bottomLeft,  insidePoint); // Bottom
        tileFrustumPlanes[4] = float4(0.0f, 0.0f, -1.0f, -minDepth);                                 // Near
        tileFrustumPlanes[5] = float4(0.0f, 0.0f,  1.0f,  maxDepth);                                 // Far
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
        float3 lightPositionVS = mul(cameraInfo.view, float4(light.position, 1.0f)).xyz;

        float distance = 0.0f;
        for (uint32_t j = 0; j < 6; ++j)
        {
            distance = dot(float4(lightPositionVS, 1.0f), tileFrustumPlanes[j]) + light.radius;
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
