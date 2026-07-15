#include "Base/Base.slang"
#include "Base/DescriptorBindings.slang"
#include "SceneRenderer/LightCullingCommon.slang"
#include "Lighting/Lights.slang"
#include "Lighting/PBRCommon.slang"
#include "Base/Utils.slang"

#ifndef GROUP_COUNT
#error "GROUP_COUNT is not defined"
#endif

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<CameraInfo> g_cameraInfo : register(b0, space0);

MIZU_VK_BINDING_SRV(0, 0)
Texture2D<half2> g_gbuffer0 : register(t0, space0);
MIZU_VK_BINDING_SRV(1, 0)
Texture2D<float4> g_gbuffer1 : register(t1, space0);
MIZU_VK_BINDING_SRV(2, 0)
Texture2D<half4> g_gbuffer2 : register(t2, space0);
MIZU_VK_BINDING_SRV(3, 0)
Texture2D<float> g_depth : register(t3, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWTexture2D<float4> g_output : register(u0, space0);

MIZU_VK_BINDING_SRV(0, 1)
StructuredBuffer<PointLight> g_pointLights : register(t0, space1);
MIZU_VK_BINDING_SRV(1, 1)
StructuredBuffer<DirectionalLight> g_directionalLights : register(t1, space1);
MIZU_VK_BINDING_SRV(2, 1)
StructuredBuffer<uint16_t> g_tileVisibleLights : register(t2, space1);
MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<LightCullingInfo> g_lightCullingInfo : register(b0, space1);

[shader("compute")]
[numthreads(GROUP_COUNT, GROUP_COUNT, 1)]
void csMain(uint3 threadId: SV_DispatchThreadID)
{
    half2 gbuffer0 = g_gbuffer0.Load(threadId);
    float4 gbuffer1 = g_gbuffer1.Load(threadId);
    half4 gbuffer2 = g_gbuffer2.Load(threadId);
    float depth = g_depth.Load(threadId);

    uint width, height;
    g_depth.GetDimensions(width, height);

    float2 uv = (float2(threadId.xy) + 0.5f) / float2(width, height);

    float3 position = worldSpacePositionFromDepth(uv, depth, g_cameraInfo.inverseViewProj);
    float3 normal = decodeNormalSpherical(float2(gbuffer0.xy));
    float3 albedo = gbuffer1.rgb;
    float metallic = float(gbuffer2.r);
    float roughness = float(gbuffer2.g);
    float ao = float(gbuffer2.b);

    PBRCommon::PBRMaterial material;
    material.position = position;
    material.N = normal;
    material.albedo = albedo;
    material.metallic = metallic;
    material.roughness = roughness;
    material.ao = ao;

    float3 V = normalize(g_cameraInfo.pos - material.position);

    float3 F0 = float3(0.04f);
    F0 = lerp(F0, material.albedo, material.metallic);

    float3 Lo = float3(0.0f);

    for (uint32_t i = 0; i < g_directionalLights.getCount(); ++i)
    {
        DirectionalLight light = g_directionalLights[i];

        float3 L = normalize(-light.direction);
        float3 radiance = light.color * light.intensity;

        Lo += PBRCommon::PBRCalculation(material, V, L, F0) * radiance;
    }

    float2 tileId = float2(threadId.xy) / float2(TILE_SIZE);
    uint32_t tileIndex = uint32_t(tileId.y) * g_lightCullingInfo.numTiles.x + uint32_t(tileId.x);
    uint32_t index = tileIndex * MAX_LIGHTS_PER_TILE;

    for (uint32_t i = 0; i < g_pointLights.getCount(); ++i)
    {
        uint16_t tileVisibleLight = g_tileVisibleLights[index + i];
        if (tileVisibleLight == INVALID_TILE_VISIBLE_LIGHT)
            break;

        LIGHT_TYPE lightType;
        uint16_t lightIndex;

        decodeTileVisibleLight(tileVisibleLight, lightType, lightIndex);

        // TODO: For the moment, all lights being culled are point lights

        PointLight light = g_pointLights[lightIndex];

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

    float3 ambient = float3(0.03f) * material.albedo * material.ao;
    float3 color = ambient + Lo;

    g_output[threadId.xy] = float4(color, 0.0f);
}
