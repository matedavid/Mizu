#include "common/Lights.slang"
#include "common/MizuBase.slang"
#include "forwardplus/LightCullingCommon.slang"

struct CameraInfo
{
    float4x4 view;
    float4x4 proj;
    float4x4 inverseViewProj;
    float3 pos;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

[[vk::binding(0, 1)]]
StructuredBuffer<PointLight> pointLights : register(t0, space1);
[[vk::binding(1, 1)]]
RWStructuredBuffer<uint32_t> visiblePointLightIndices : register(u0, space2);
[[vk::binding(2, 1)]]
ConstantBuffer<LightCullingInfo> lightCullingInfo : register(b0, space2);

[[vk::binding(0, 2)]]
Texture2D<float> depthTexture : register(t0, space3);
[[vk::binding(1, 2)]]
SamplerState depthTextureSampler : register(s0, space3);

groupshared uint32_t tileMinDepth;
groupshared uint32_t tileMaxDepth;
groupshared uint32_t tileVisibleLightCount;
groupshared float4 tileFrustumPlanes[6];
groupshared uint32_t tileVisibleLightIndices[MAX_LIGHTS_PER_TILE];

float4 makePlane(float3 a, float3 b, float3 c)
{
    float3 normal = normalize(cross(b - a, c - a));
    return float4(normal, -dot(normal, a));
}

float linearizeDepth(float depth, float4x4 proj)
{
    float A = proj._33;
    float B = proj._34;

    float nearPlane = B / (A - 1.0f);
    float farPlane = B / (A + 1.0f);

    float z = depth * 2.0f - 1.0f;

    return (2.0f * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
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

    float2 texCoords = float2(float(threadId.x) / float(width), float(threadId.y) / float(height));
    float depth = depthTexture.SampleLevel(depthTextureSampler, texCoords, 0);

    float linearDepth = linearizeDepth(depth, cameraInfo.proj);

    uint32_t uintLinearDepth = asuint(linearDepth);
    InterlockedMin(tileMinDepth, uintLinearDepth);
    InterlockedMax(tileMaxDepth, uintLinearDepth);

    GroupMemoryBarrierWithGroupSync();

    // 3. Calculate tile frustums

    if (groupIndex == 0)
    {
        float2 ndcSizePerTile = 2.0f * float2(TILE_SIZE) / float2(width, height);

        const float2 ndcUpperLeft = float2(-1.0f, -1.0f);

        float minDepth = asfloat(tileMinDepth);
        float maxDepth = asfloat(tileMaxDepth);

        if (minDepth >= maxDepth)
            minDepth = maxDepth;

        float2 ndcCorners[4];
        // Upper left
        ndcCorners[0] = float2(ndcUpperLeft + groupId.xy * ndcSizePerTile);
        // Upper right
        ndcCorners[1] = float2(ndcCorners[0].x + ndcSizePerTile.x, ndcCorners[0].y);
        // Bottom right
        ndcCorners[2] = ndcCorners[0] + ndcSizePerTile;
        // Bottom left
        ndcCorners[3] = float2(ndcCorners[0].x, ndcCorners[0].y + ndcSizePerTile.y);

        float3 worldSpacePoints[8];
        for (uint32_t i = 0; i < 4; ++i)
        {
            float4 temp = mul(cameraInfo.inverseViewProj, float4(ndcCorners[i], minDepth, 1.0f));
            worldSpacePoints[i] = temp.xyz / temp.w;

            temp = mul(cameraInfo.inverseViewProj, float4(ndcCorners[i], maxDepth, 1.0f));
            worldSpacePoints[i + 4] = temp.xyz / temp.w;
        }

        // left = (camera pos, near upper left, far upper left)
        tileFrustumPlanes[0] = makePlane(cameraInfo.pos, worldSpacePoints[0], worldSpacePoints[4]);
        // right = (camera pos, far upper right, near upper right)
        tileFrustumPlanes[1] = makePlane(cameraInfo.pos, worldSpacePoints[5], worldSpacePoints[1]);
        // top = (camera pos, near upper right, near upper left)
        tileFrustumPlanes[2] = makePlane(cameraInfo.pos, worldSpacePoints[1], worldSpacePoints[0]);
        // bottom = (camera pos, near bottom left, near bottom right)
        tileFrustumPlanes[3] = makePlane(cameraInfo.pos, worldSpacePoints[3], worldSpacePoints[2]);

        // near = (near upper left, near upper right, near bottom right)
        tileFrustumPlanes[4] = makePlane(worldSpacePoints[0], worldSpacePoints[1], worldSpacePoints[2]);
        // far = (far bottom right, far upper right, far upper left)
        tileFrustumPlanes[5] = makePlane(worldSpacePoints[6], worldSpacePoints[5], worldSpacePoints[4]);

        // Make sure normals face inward (towards camera)
        for (uint32_t i = 0; i < 6; ++i)
        {
            if (dot(tileFrustumPlanes[i].xyz, cameraInfo.pos) + tileFrustumPlanes[i].w < 0.0f)
            {
                tileFrustumPlanes[i].xyz *= -1.0f;
                tileFrustumPlanes[i].w *= -1.0f;
            }
        }
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

        bool visible = true;

        for (uint32_t i = 0; i < 6; ++i)
        {
            if (dot(light.position, tileFrustumPlanes[i].xyz) + tileFrustumPlanes[i].w < -light.radius)
            {
                visible = false;
                break;
            }
        }

        if (visible)
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
