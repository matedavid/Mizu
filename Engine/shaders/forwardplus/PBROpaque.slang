#include "common/MizuBase.slang"

//
// Vertex
//

struct CameraInfo
{
    float4x4 view;
    float4x4 proj;
    float4x4 inverseViewProj;
    float3 pos;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

struct VertexInput
{
    float3 position;
    float3 normal;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 worldPosition;
    float3 normal;
    float2 texCoord;
}

struct ModelInfo
{
    float4x4 model;
}

[push_constant]
ModelInfo modelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    float4 pos = float4(input.position, 1.0f);

    // TODO: Should probably move this calculation outside of the vertex shader
    float3x3 normalMatrix = transpose(inverse(float3x3(mul(cameraInfo.view, modelInfo.model))));

    VertexOutput output;
    output.position = mul(cameraInfo.proj, mul(cameraInfo.view, mul(modelInfo.model, pos)));
    output.worldPosition = mul(modelInfo.model, pos);
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

[shader("fragment")]
float4 fsMain(VertexOutput input)
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

    float3 V = normalize(cameraInfo.pos - input.worldPosition.xyz);

    float3 F0 = float3(0.04f);
    F0 = lerp(F0, material.albedo, material.metallic);

    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    for (uint32_t i = 0; i < directionalLights.getCount(); ++i)
    {
        DirectionalLight light = directionalLights[i];

        float3 L = normalize(-light.direction);
        float3 radiance = light.color * light.intensity;

        float3 lightDir = normalize(-light.direction);
        float diff = max(dot(material.N, lightDir), 0.0);

        float3 color = PBRCommon::PBRCalculation(material, V, L, F0) * radiance;
        Lo += color;
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
        float distance = length(light.position - input.worldPosition.xyz);

        const float startFadingFactor = 0.80f;

        float fadeFactor = 1.0f;
        if (distance > light.radius * startFadingFactor)
        {
            fadeFactor = 1.0f - smoothstep(light.radius * startFadingFactor, light.radius, distance);
        }

        float attenuation = 1.0f / (distance * distance + 1e-5);
        attenuation *= fadeFactor;

        float diff = max(dot(material.N, L), 0.0f);

        Lo += PBRCommon::PBRCalculation(material, V, L, F0) * light.intensity * diff * attenuation;
    }

    float3 ambient = float3(0.03f) * Lo * material.ao;
    float3 color = ambient + Lo;

    return float4(color, 1.0f);
}

