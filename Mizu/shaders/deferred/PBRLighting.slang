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
StructuredBuffer<PointLight> pointLights : register(t0, space2);

struct DirectionalLight
{
    float4 position;
    float4 direction;
    float3 color;
    float intensity;

    bool castShadows;
}
[[vk::binding(3, 0)]]
StructuredBuffer<DirectionalLight> directionalLights : register(t0, space3);

MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, albedo, 2, 0);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, position, 2, 1);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, normal, 2, 2);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, metallicRoughnessAO, 2, 3);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float4 metallicRoughnessAOValues = metallicRoughnessAO.Sample(_metallicRoughnessAOSampler, input.texCoord);

    PBRCommon::PBRMaterial material;
    material.position = position.Sample(_positionSampler, input.texCoord).rgb;
    material.N = normal.Sample(_normalSampler, input.texCoord).rgb;
    material.albedo = albedo.Sample(_albedoSampler, input.texCoord).rgb;
    material.metallic = metallicRoughnessAOValues.r;
    material.roughness = metallicRoughnessAOValues.g;

    float3 V = normalize(input.cameraPosition - material.position);

    float3 F0 = float3(0.04);
    F0 = lerp(F0, material.albedo, material.metallic);

    float3 Lo = float3(0.0);

    // Directional Lights
    for (uint32_t i = 0; i < directionalLights.getCount(); ++i)
    {
        DirectionalLight light = directionalLights[i];

        float3 L = normalize(-light.direction.xyz);
        float3 radiance = light.color.rgb * light.intensity;

        Lo += PBRCommon::PBRCalculation(material, V, L, F0) * radiance;
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

    float3 ambient = float3(0.03) * material.albedo; // * ao;
    float3 color = ambient + Lo;

    return float4(color, 1.0);
}
