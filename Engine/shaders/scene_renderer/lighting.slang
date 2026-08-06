#include "base/base.slang"
#include "base/descriptor_bindings.slang"
#include "scene_renderer/light_culling_common.slang"
#include "lighting/lights.slang"
#include "lighting/pbr_common.slang"
#include "base/utils.slang"

#ifndef GROUP_COUNT
#error "GROUP_COUNT is not defined"
#endif

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<CameraInfo> g_camera_info : register(b0, space0);

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
StructuredBuffer<PointLight> g_point_lights : register(t0, space1);
MIZU_VK_BINDING_SRV(1, 1)
StructuredBuffer<DirectionalLight> g_directional_lights : register(t1, space1);
MIZU_VK_BINDING_SRV(2, 1)
StructuredBuffer<uint16_t> g_tile_visible_lights : register(t2, space1);
MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<LightCullingInfo> g_light_culling_info : register(b0, space1);

MIZU_VK_BINDING_SRV(3, 1)
Texture2D<float> g_directional_shadow_map : register(t3, space1);
MIZU_VK_BINDING_SRV(4, 1)
StructuredBuffer<float> g_cascade_splits : register(t4, space1);
MIZU_VK_BINDING_SRV(5, 1)
StructuredBuffer<float4x4> g_light_space_matrices : register(t5, space1);
MIZU_VK_BINDING_SAMPLER(0, 1)
SamplerState g_shadow_map_sampler : register(s0, space1);

float transform_to_cascaded_shadow_map_position(float value, uint32_t idx, uint32_t num)
{
    float step_size = 1.0f / float(num);
    float v = value * step_size + idx * step_size;

    float min_value = float(idx) * step_size;
    float max_value = float(idx + 1) * step_size;

    return clamp(v, min_value, max_value);
}

float shadow_visibility(uint32_t light_idx, float3 position_ws, float3 normal, float3 direction)
{
    uint32_t width, height;
    g_directional_shadow_map.GetDimensions(width, height);

    uint32_t num_cascades = g_cascade_splits.getCount();
    uint32_t num_lights = g_light_space_matrices.getCount() / num_cascades;

    float4 position_vs = mul(g_camera_info.view, float4(position_ws, 1.0f));

    uint32_t cascade_idx = 0;
    for (uint32_t i = 0; i < num_cascades - 1; ++i)
    {
        if (position_vs.z < g_cascade_splits[i])
        {
            cascade_idx = i + 1;
        }
    }

    float4x4 light_space_matrix = g_light_space_matrices[cascade_idx];
    float4 position_ls = mul(light_space_matrix, float4(position_ws, 1.0f));

    float3 proj_coords = position_ls.xyz / position_ls.w;
    proj_coords.xy = proj_coords.xy * 0.5f + 0.5f;

    proj_coords.x = transform_to_cascaded_shadow_map_position(proj_coords.x, cascade_idx, num_cascades);
    proj_coords.y = transform_to_cascaded_shadow_map_position(proj_coords.y, light_idx, num_lights);

    // Invert y because we rasterize with NDC Y up
    proj_coords.y = 1.0f - proj_coords.y;

    proj_coords.z = clamp(proj_coords.z, 0.0f, 1.0f);

    float current_depth = proj_coords.z;

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
            float sx = proj_coords.x + dx * x;
            float sy = proj_coords.y + dy * y;

            float closest_depth = g_directional_shadow_map.SampleLevel(g_shadow_map_sampler, float2(sx, sy), 0);
            shadow += current_depth - bias > closest_depth ? 1.0f : 0.0f;

            count += 1;
        }
    }

    return 1.0f - shadow / float(count);
}

[shader("compute")]
[numthreads(GROUP_COUNT, GROUP_COUNT, 1)]
void cs_main(uint3 thread_id: SV_DispatchThreadID)
{
    half2 gbuffer0 = g_gbuffer0.Load(thread_id);
    float4 gbuffer1 = g_gbuffer1.Load(thread_id);
    half4 gbuffer2 = g_gbuffer2.Load(thread_id);
    float depth = g_depth.Load(thread_id);

    uint width, height;
    g_depth.GetDimensions(width, height);

    float2 uv = (float2(thread_id.xy) + 0.5f) / float2(width, height);

    float3 position = world_space_position_from_depth(uv, depth, g_camera_info.inverse_view_proj);
    float3 normal = decode_normal_spherical(float2(gbuffer0.xy));
    float3 albedo = gbuffer1.rgb;
    float metallic = float(gbuffer2.r);
    float roughness = float(gbuffer2.g);
    float ao = float(gbuffer2.b);

    PbrCommon::PbrMaterial material;
    material.position = position;
    material.n = normal;
    material.albedo = albedo;
    material.metallic = metallic;
    material.roughness = roughness;
    material.ao = ao;

    float3 v = normalize(g_camera_info.pos - material.position);

    float3 f0 = float3(0.04f);
    f0 = lerp(f0, material.albedo, material.metallic);

    float3 lo = float3(0.0f);

    uint32_t directional_shadow_casting_light_idx = 0;
    for (uint32_t i = 0; i < g_directional_lights.getCount(); ++i)
    {
        DirectionalLight light = g_directional_lights[i];

        float3 l = normalize(-light.direction);
        float3 radiance = light.color * light.intensity;

        float shadow = 1.0f;
        if (light.cast_shadows > 0.5f)
        {
            shadow = shadow_visibility(
                directional_shadow_casting_light_idx, material.position, material.n, light.direction);
        }

        lo += PbrCommon::pbr_calculation(material, v, l, f0) * radiance * shadow;

        directional_shadow_casting_light_idx += uint32_t(light.cast_shadows > 0.5f);
    }

    float2 tile_id = float2(thread_id.xy) / float2(TILE_SIZE);
    uint32_t tile_index = uint32_t(tile_id.y) * g_light_culling_info.num_tiles.x + uint32_t(tile_id.x);
    uint32_t index = tile_index * MAX_LIGHTS_PER_TILE;

    for (uint32_t i = 0; i < g_point_lights.getCount(); ++i)
    {
        uint16_t tile_visible_light = g_tile_visible_lights[index + i];
        if (tile_visible_light == INVALID_TILE_VISIBLE_LIGHT)
            break;

        LIGHT_TYPE light_type;
        uint16_t light_index;

        decode_tile_visible_light(tile_visible_light, light_type, light_index);

        // TODO: For the moment, all lights being culled are point lights

        PointLight light = g_point_lights[light_index];

        float3 l = normalize(light.position - material.position);
        float light_distance = length(light.position - material.position);

        const float start_fading_factor = 0.80f;

        float fade_factor = 1.0f;
        if (light_distance > light.radius * start_fading_factor)
        {
            fade_factor = 1.0f - smoothstep(light.radius * start_fading_factor, light.radius, light_distance);
        }

        float attenuation = 1.0f / (light_distance * light_distance + 1e-5) * fade_factor;
        float3 radiance = light.color * light.intensity * attenuation;

        lo += PbrCommon::pbr_calculation(material, v, l, f0) * radiance;
    }

    float3 ambient = float3(0.03f) * material.albedo * material.ao;
    float3 color = ambient + lo;

    g_output[thread_id.xy] = float4(color, 0.0f);
}
