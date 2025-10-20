#include "common/MizuBase.slang"

//
// Vertex
//

#include "common/FrameInfo.slang"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 worldPosition : POSITION1;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

struct InstanceTransformInfo
{
    float4x4 transform;
    float4x4 normal_matrix;
};

[[vk::binding(1, 0)]]
StructuredBuffer<InstanceTransformInfo> transformInfo : register(b1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<uint64_t> transformIndices : register(b2, space0);

struct PushConstant
{
    uint64_t transformOffset;
}

[push_constant]
PushConstant pushConstant;

[shader("vertex")]
VertexOutput vsMain(VertexInput input, uint32_t instanceId: SV_InstanceID)
{
    float4 pos = float4(input.position, 1.0f);

    uint64_t transformIndex = transformIndices[pushConstant.transformOffset + instanceId];
    InstanceTransformInfo transformInfo = transformInfo[transformIndex];

    float4x4 modelMatrix = transformInfo.transform;
    float3x3 normalMatrix = float3x3(transformInfo.normal_matrix);

    VertexOutput output;
    output.position = mul(cameraInfo.viewProj, mul(modelMatrix, pos));
    output.worldPosition = mul(modelMatrix, pos);
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

#include "common/Lights.slang"
#include "common/PBRCommon.slang"
#include "forwardplus/LightCullingCommon.slang"

[[vk::binding(0, 1)]]
StructuredBuffer<PointLight> pointLights : register(t0, space1);
[[vk::binding(1, 1)]]
StructuredBuffer<DirectionalLight> directionalLights : register(t1, space1);
[[vk::binding(2, 1)]]
StructuredBuffer<uint32_t> visiblePointLightIndices : register(t2, space1);
[[vk::binding(3, 1)]]
ConstantBuffer<LightCullingInfo> lightCullingInfo : register(b0, space1);

[[vk::binding(4, 1)]]
Texture2D<float> directionalShadowMap : register(t3, space1);
[[vk::binding(5, 1)]]
SamplerState directionalShadowMapSampler : register(s0, space1);
[[vk::binding(6, 1)]]
StructuredBuffer<float> cascadeSplits : register(t4, space1);
[[vk::binding(7, 1)]]
StructuredBuffer<float4x4> lightSpaceMatrices : register(t5, space1);

[[vk::binding(0, 2)]]
Texture2D<float4> albedo : register(t0, space2);
[[vk::binding(1, 2)]]
Texture2D<float4> metallic : register(t1, space2);
[[vk::binding(2, 2)]]
Texture2D<float4> roughness : register(t2, space2);
[[vk::binding(3, 2)]]
Texture2D<float4> ambientOcclusion : register(t3, space2);
[[vk::binding(4, 2)]]
SamplerState sampler : register(s0, space2);

float transformToCascadedShadowMapPosition(float value, uint32_t idx, uint32_t num)
{
    float step = 1.0f / float(num);
    float v = value * step + idx * step;

    float min = float(idx) * step;
    float max = float(idx + 1) * step;

    return clamp(v, min, max);
}

float shadowVisibility(uint32_t lightIdx, float3 positionWS, float3 normal, float3 direction, out float3 debugColor)
{
    uint32_t width, height;
    directionalShadowMap.GetDimensions(width, height);

    uint32_t numCascades = cascadeSplits.getCount();
    uint32_t numLights = lightSpaceMatrices.getCount() / numCascades;

    float4 positionVS = mul(cameraInfo.view, float4(positionWS, 1.0f));

    uint32_t cascadeIdx = 0;
    for (uint32_t i = 0; i < numCascades - 1; ++i)
    {
        if (positionVS.z < cascadeSplits[i])
        {
            cascadeIdx = i + 1;
        }
    }

    if (cascadeIdx == 0)
        debugColor = float3(1.0f, 0.0f, 0.0f) * 0.5f;
    else if (cascadeIdx == 1)
        debugColor = float3(0.0f, 1.0f, 0.0f) * 0.5f;
    else if (cascadeIdx == 2)
        debugColor = float3(0.0f, 0.0f, 1.0f) * 0.5f;
    else if (cascadeIdx == 3)
        debugColor = float3(1.0f, 0.0f, 1.0f) * 0.5f;

    float4x4 lightSpaceMatrix = lightSpaceMatrices[cascadeIdx];
    float4 positionLS = mul(lightSpaceMatrix, float4(positionWS, 1.0f));

    float3 projCoords = positionLS.xyz / positionLS.w;
    projCoords.xy = projCoords.xy * 0.5f + 0.5f;

    projCoords.x = transformToCascadedShadowMapPosition(projCoords.x, cascadeIdx, numCascades);
    projCoords.y = transformToCascadedShadowMapPosition(projCoords.y, lightIdx, numLights);

    // TODO: Seems like a hack, but without this clamp I was seeing artifacts on some parts
    projCoords.z = clamp(projCoords.z, 0.0f, 1.0f);

    float currentDepth = projCoords.z;

    float scale = 0.75f;
    float dx = scale * 1.0f / float(width);
    float dy = scale * 1.0f / float(height);

    float shadow = 0.0f;
    uint32_t count = 0;
    const int32_t range = 1;

    float bias = max(0.005f * (1.0f - dot(normal, -direction)), 0.0005f);

    for (int32_t x = -range; x <= range; ++x)
    {
        for (int32_t y = -range; y <= range; ++y)
        {
            float x = projCoords.x + dx * x;
            float y = projCoords.y + dy * y;

            float closestDepth = directionalShadowMap.Sample(directionalShadowMapSampler, float2(x, y));
            shadow += currentDepth - bias > closestDepth ? 1.0f : 0.0f;

            count += 1;
        }
    }

    return 1.0f - shadow / float(count);
}

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target0
{
    float metallicValue = metallic.Sample(sampler, input.texCoord).r;
    float roughnessValue = roughness.Sample(sampler, input.texCoord).g;
    float ambientOcclusionValue = ambientOcclusion.Sample(sampler, input.texCoord).b;

    PBRCommon::PBRMaterial material;
    material.position = input.worldPosition.xyz;
    material.N = normalize(input.normal);
    material.albedo = albedo.Sample(sampler, input.texCoord).rgb;
    material.metallic = metallicValue;
    material.roughness = roughnessValue;
    material.ao = ambientOcclusionValue;

    float3 V = normalize(cameraInfo.pos - material.position);

    float3 F0 = float3(0.04f);
    F0 = lerp(F0, material.albedo, material.metallic);

    float3 Lo = float3(0.0f);

    uint32_t directionalShadowCastingLightIdx = 0;
    for (uint32_t i = 0; i < directionalLights.getCount(); ++i)
    {
        DirectionalLight light = directionalLights[i];

        float3 L = normalize(-light.direction);
        float3 radiance = light.color * light.intensity;

        float shadow = 1.0f;
        float3 debugColor = float3(1.0f);
        if (light.castShadows)
        {
            shadow = shadowVisibility(
                directionalShadowCastingLightIdx,
                input.worldPosition.xyz,
                input.normal,
                light.direction.xyz,
                debugColor);
        }

        Lo += PBRCommon::PBRCalculation(material, V, L, F0) * radiance * shadow;
        // Lo += max(shadow, 1.0f) * debugColor;
    }

    // SV_Position in hlsl fragment shader, represents the position in screen space
    float2 tileId = input.position.xy / float2(TILE_SIZE);
    uint32_t tileIndex = uint32_t(tileId.y) * lightCullingInfo.numTiles.x + uint32_t(tileId.x);
    uint32_t index = tileIndex * MAX_LIGHTS_PER_TILE;

    for (uint32_t i = 0; i < MAX_LIGHTS_PER_TILE; ++i)
    {
        uint32_t lightIndex = visiblePointLightIndices[index + i];
        if (lightIndex == 0xFFFFFFFF)
            break;

        PointLight light = pointLights[lightIndex];

        float3 L = normalize(light.position - material.position);
        float distance = length(light.position - material.position);

        const float startFadingFactor = 0.80f;

        float fadeFactor = 1.0f;
        if (distance > light.radius * startFadingFactor)
        {
            fadeFactor = 1.0f - smoothstep(light.radius * startFadingFactor, light.radius, distance);
        }

        float attenuation = 1.0f / (distance * distance + 1e-5) * fadeFactor;
        float3 radiance = light.color * light.intensity * attenuation;

        Lo += PBRCommon::PBRCalculation(material, V, L, F0) * radiance;
    }

    float3 ambient = float3(0.03f) * Lo * material.ao;
    float3 color = ambient + Lo;

    return float4(color, 1.0f);
}

