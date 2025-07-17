//
// Vertex
//

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

struct CameraInfo
{
    float4x4 view;
    float4x4 proj;
    float4x4 inverseViewProj;
    float3 pos;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

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

    VertexOutput output;
    output.position = mul(cameraInfo.proj, mul(cameraInfo.view, mul(modelInfo.model, pos)));
    output.worldPosition = mul(modelInfo.model, pos);
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

#include "common/Lights.slang"
#include "forwardplus/LightCullingCommon.slang"

[[vk::binding(0, 1)]]
StructuredBuffer<PointLight> pointLights : register(t0, space1);
[[vk::binding(1, 1)]]
StructuredBuffer<DirectionalLight> directionalLights : register(t1, space1);
[[vk::binding(2, 1)]]
StructuredBuffer<uint32_t> visiblePointLightIndices : register(t2, space1);
[[vk::binding(3, 1)]]
ConstantBuffer<LightCullingInfo> lightCullingInfo : register(b0, space1);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(cameraInfo.pos - input.worldPosition.xyz);

    float3 Lo = float3(0, 0, 0);
    for (uint32_t i = 0; i < directionalLights.getCount(); ++i)
    {
        DirectionalLight light = directionalLights[i];

        float3 lightDir = normalize(-light.direction);
        float diff = max(dot(normal, lightDir), 0.0);

        float3 diffuse = light.intensity * light.color * diff;
        Lo += diffuse;
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

        float3 lightDir = normalize(light.position - input.worldPosition.xyz);
        float distance = length(light.position - input.worldPosition.xyz);

        const float startFadingFactor = 0.80f;

        float fadeFactor = 1.0;
        if (distance > light.radius * startFadingFactor)
        {
            fadeFactor = 1.0 - smoothstep(light.radius * startFadingFactor, light.radius, distance);
        }

        float attenuation = 1.0f / (distance * distance + 1e-5);
        attenuation *= fadeFactor;

        float diff = max(dot(normal, lightDir), 0.0);

        float3 diffuse = light.intensity * light.color * diff * attenuation;
        Lo += diffuse;
    }

    return float4(Lo, 1.0f);
}
