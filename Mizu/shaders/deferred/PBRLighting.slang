#include "common/MizuBase.slang"
#include "common/PBRCommon.slang"

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
    float3 cameraPosition;
}

struct CameraInfo
{
    float4x4 view;
    float4x4 projection;
    float3 cameraPosition;
}

ParameterBlock<CameraInfo> uCameraInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texCoord = input.texCoord;
    output.cameraPosition = uCameraInfo.cameraPosition;

    return output;
}

//
// Fragment
//

MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, albedo, 1, 0);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, position, 1, 1);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, normal, 1, 2);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, metallicRoughnessAO, 1, 3);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    // TODO: Not hardcode
    float3 lightPositions[] = {
        float3(0.0, 0.0, 2.0),
        float3(5.0, 0.0, 2.0),
        float3(0.0, 5.0, 2.0),
        float3(5.0, 5.0, 2.0),
    };
    float3 lightColor = float3(1.0, 1.0, 1.0);
    // ======

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

    // TODO: Should loop through all lights
    for (int i = 0; i < 4; ++i)
    {
        float3 L = normalize(lightPositions[i] - worldPosition);
        float3 H = normalize(V + L);

        float distance = length(lightPositions[i] - worldPosition);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = lightColor * attenuation;

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
