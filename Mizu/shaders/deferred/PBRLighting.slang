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
[[vk::binding(0, 1)]]
StructuredBuffer<PointLight> pointLights : register(t0, space1);

MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, albedo, 2, 0);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, position, 2, 1);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, normal, 2, 2);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, metallicRoughnessAO, 2, 3);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float3 albedo = albedo.Sample(_albedoSampler, input.texCoord).rgb;
    float3 worldPosition = position.Sample(_positionSampler, input.texCoord).rgb;

    float4 metallicRoughnessAOValues = metallicRoughnessAO.Sample(_metallicRoughnessAOSampler, input.texCoord);
    float metallic = metallicRoughnessAOValues.r;
    float roughness = metallicRoughnessAOValues.g;

    float3 N = normal.Sample(_normalSampler, input.texCoord).rgb;
    float3 V = normalize(input.cameraPosition - worldPosition);

    float3 F0 = float3(0.04);
    F0 = lerp(F0, albedo, metallic);

    float3 Lo = float3(0.0);

    for (uint32_t i = 0; i < pointLights.getCount(); ++i)
    {
        PointLight pointLight = pointLights[i];

        float3 L = normalize(pointLight.position.xyz - worldPosition);
        float3 H = normalize(V + L);

        float distance = length(pointLight.position.xyz - worldPosition);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = pointLight.color.rgb * pointLight.intensity * attenuation;

        // cook-torrance brdf
        float NDF = PBRCommon::distributionGGX(N, H, roughness);
        float G = PBRCommon::geometrySmith(N, V, L, roughness);
        float3 F = PBRCommon::fresnelSchlick(saturate(dot(H, V)), F0);

        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        kD *= 1.0 - metallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    float3 ambient = float3(0.03) * albedo; // * ao;
    float3 color = ambient + Lo;

    return float4(color, 1.0);
}
