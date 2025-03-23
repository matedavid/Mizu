#include "common/MizuBase.slang"
#include "common/PBRCommon.slang"

//
// Vertex
//

struct CameraInfo
{
    float4x4 view;
    float4x4 projection;
    float3 cameraPosition;
}

ConstantBuffer<CameraInfo> cameraInfo : register(b1, space0);

struct VertexInput
{
    float3 position;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoord;
    float3 cameraPosition;
}

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texCoord = input.texCoord;
    output.cameraPosition = cameraInfo.cameraPosition;

    return output;
}

//
// Fragment
//

struct PointLight
{
    float4 position;
    float4 color;
    float intensity;
}
[[vk::binding(2, 0)]]
StructuredBuffer<PointLight> pointLights : register(t2, space0);

struct DirectionalLight
{
    float4 position;
    float4 direction;
    float3 color;
    float intensity;

    bool castShadows;
}
[[vk::binding(3, 0)]]
StructuredBuffer<DirectionalLight> directionalLights : register(t3, space0);

[[vk::binding(0, 1)]]
Texture2D<float> directionalShadowmaps : register(t0, space1);
[[vk::binding(1, 1)]]
StructuredBuffer<float4x4> directionalLightSpaceMatrices : register(t1, space1);
[[vk::binding(2, 1)]]
StructuredBuffer<float> directionalCascadeSplits : register(t2, space1);

Texture2D<float4> albedo : register(t0, space2);
Texture2D<float4> position : register(t1, space2);
Texture2D<float4> normal : register(t2, space2);
Texture2D<float4> metallicRoughnessAO : register(t3, space2);

[[vk::binding(4, 2)]]
SamplerState sampler : register(s0, space2);

float cascadedShadowCalculation(float3 posWorldSpace, float3 normal, DirectionalLight light, int lightIdx)
{
    float4 posViewSpace = mul(cameraInfo.view, float4(posWorldSpace, 1.0f));

    // Select cascade
    uint32_t numCascades = directionalCascadeSplits.getCount();

    uint32_t cascadeIdx = 0;
    for (uint32_t i = 0; i < numCascades - 1; ++i)
    {
        if (posViewSpace.z < directionalCascadeSplits[i])
        {
            cascadeIdx = i + 1;
        }
    }

    uint32_t numLights = directionalLightSpaceMatrices.getCount() / numCascades;

    uint32_t idx = cascadeIdx * numLights + lightIdx;
    float4x4 lightSpaceMatrix = directionalLightSpaceMatrices[idx];

    float4 posLightSpace = mul(lightSpaceMatrix, float4(posWorldSpace, 1.0f));

    // Perspective division
    float3 projCoords = posLightSpace.xyz / posLightSpace.w;

    // If perspective division ends up outside of NDC coordinates, do not continue
    if (projCoords.x <= -1.0f || projCoords.x >= 1.0f || projCoords.y <= -1.0f || projCoords.y >= 1.0f
        || projCoords.z <= 0.0f || projCoords.z >= 1.0f)
    {
        return 0.0f;
    }

    // Transform to [0,1] range
    projCoords.xy = projCoords.xy * 0.5f + 0.5f;

    // Move coordinate to adecuate shadow map
    float tileSizeX = 1.0f / float(numLights);
    float tileSizeY = 1.0f / float(numCascades);
    projCoords.x = projCoords.x * tileSizeX + tileSizeX * lightIdx;
    projCoords.y = projCoords.y * tileSizeY + tileSizeY * cascadeIdx;

    // Clamp to minimum and maximum values
    float minX = lightIdx * tileSizeX;
    float maxX = (lightIdx + 1) * tileSizeX;

    projCoords.x = clamp(projCoords.x, minX, maxX);

    float minY = cascadeIdx * tileSizeY;
    float maxY = (cascadeIdx + 1) * tileSizeY;

    projCoords.y = clamp(projCoords.y, minY, maxY);

    // Get closest depth value from light's perspective
    float closestDepth = directionalShadowmaps.Sample(sampler, projCoords.xy);

    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;

    // Check whether current frag pos is in shadow with given bias
    float bias = max(0.005f * (1.0f - dot(normal, -light.direction.xyz)), 0.0005f);
    float shadow = currentDepth - bias > closestDepth ? 1.0f : 0.0f;

    return shadow;
}

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float4 metallicRoughnessAOValues = metallicRoughnessAO.Sample(sampler, input.texCoord);

    PBRCommon::PBRMaterial material;
    material.position = position.Sample(sampler, input.texCoord).rgb;
    material.N = normal.Sample(sampler, input.texCoord).rgb;
    material.albedo = albedo.Sample(sampler, input.texCoord).rgb;
    material.metallic = metallicRoughnessAOValues.r;
    material.roughness = metallicRoughnessAOValues.g;
    material.ao = metallicRoughnessAOValues.b;

    float3 V = normalize(input.cameraPosition - material.position);

    float3 F0 = float3(0.04);
    F0 = lerp(F0, material.albedo, material.metallic);

    float3 Lo = float3(0.0);

    // Directional Lights
    uint32_t shadowDirectionalLight = 0;
    for (uint32_t i = 0; i < directionalLights.getCount(); ++i)
    {
        DirectionalLight light = directionalLights[i];

        float3 L = normalize(-light.direction.xyz);
        float3 radiance = light.color.rgb * light.intensity;

        float3 color = PBRCommon::PBRCalculation(material, V, L, F0) * radiance;

        if (light.castShadows && shadowDirectionalLight < directionalLightSpaceMatrices.getCount())
        {
            float shadow = cascadedShadowCalculation(material.position, material.N, light, shadowDirectionalLight);
            Lo += color * (shadow == 1.0 ? float3(0.04) : float3(1.0));

            shadowDirectionalLight += 1;
        }
        else
        {
            Lo += color;
        }
    }

    // Point Lights
    for (uint32_t i = 0; i < pointLights.getCount(); ++i)
    {
        PointLight light = pointLights[i];

        float3 L = normalize(light.position.xyz - material.position);

        float distance = length(light.position.xyz - material.position);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = light.color.rgb * light.intensity * attenuation;

        Lo += PBRCommon::PBRCalculation(material, V, L, F0) * radiance;
    }

    // float3 ambient = float3(0.0f);
    float3 ambient = float3(0.03f) * material.albedo * material.ao;
    float3 color = ambient + Lo;

    return float4(color, 1.0);
}
