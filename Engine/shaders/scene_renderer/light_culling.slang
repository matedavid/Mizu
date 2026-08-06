#include "base/base.slang"
#include "base/descriptor_bindings.slang"
#include "base/utils.slang"
#include "scene_renderer/light_culling_common.slang"
#include "lighting/lights.slang"

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<CameraInfo> g_camera_info : register(b0, space0);

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<PointLight> g_point_lights : register(t0, space0);
MIZU_VK_BINDING_CBV(1, 0)
ConstantBuffer<LightCullingInfo> g_light_culling_info : register(b1, space0);
MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<uint16_t> g_tile_visible_lights : register(u0, space0);

MIZU_VK_BINDING_SRV(1, 0)
Texture2D<float> g_depth : register(t1, space0);

groupshared uint32_t tile_min_depth;
groupshared uint32_t tile_max_depth;
groupshared uint32_t tile_visible_light_count;
groupshared float4 tile_frustum_planes[6];
groupshared uint16_t tile_visible_lights[MAX_LIGHTS_PER_TILE];

float4 create_plane_from_points(float3 point0, float3 point1, float3 point2, float3 inside_point)
{
    float3 normal = normalize(cross(point1 - point0, point2 - point0));
    float4 plane = float4(normal, -dot(normal, point0));

    if (dot(float4(inside_point, 1.0f), plane) < 0.0f)
    {
        plane *= -1.0f;
    }

    return plane;
}

float linearize_depth(float depth, float znear, float zfar)
{
    return znear * zfar / (zfar + depth * (znear - zfar));
}

[shader("compute")]
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void cs_main(uint32_t group_index: SV_GroupIndex, uint3 group_id: SV_GroupID, uint3 thread_id: SV_DispatchThreadID)
{
    // Compute values
    uint32_t width, height;
    g_depth.GetDimensions(width, height);

    uint32_t tile_index = group_id.y * g_light_culling_info.num_tiles.x + group_id.x;

    // 1. Initialize per-tile information

    if (group_index == 0)
    {
        tile_min_depth = 0xFFFFFFFF;
        tile_max_depth = 0;
        tile_visible_light_count = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    // 2. Calculate tile min and max depth values

    if (thread_id.x < width && thread_id.y < height)
    {
        float2 tex_coords = float2(thread_id.xy) / float2(width, height);
        float depth = g_depth.Load(thread_id);

        float linear_depth = linearize_depth(depth, g_camera_info.znear, g_camera_info.zfar);

        uint32_t uint_linear_depth = asuint(linear_depth);
        InterlockedMin(tile_min_depth, uint_linear_depth);
        InterlockedMax(tile_max_depth, uint_linear_depth);
    }

    GroupMemoryBarrierWithGroupSync();

    // 3. Calculate tile frustums

    if (group_index == 0)
    {
        float min_depth = asfloat(tile_min_depth) - 0.01f;
        float max_depth = asfloat(tile_max_depth) + 0.01f;

        uint2 tile_min_pixel = group_id.xy * TILE_SIZE;
        uint2 tile_max_pixel = min(tile_min_pixel + uint2(TILE_SIZE), uint2(width, height));

        float2 tile_min_uv = float2(tile_min_pixel) / float2(width, height);
        float2 tile_max_uv = float2(tile_max_pixel) / float2(width, height);
        float2 tile_center_uv = 0.5f * (tile_min_uv + tile_max_uv);

        float3 top_left = view_space_position_from_depth(
            float2(tile_min_uv.x, tile_max_uv.y), 1.0f, g_camera_info.inverse_proj);
        float3 top_right = view_space_position_from_depth(
            float2(tile_max_uv.x, tile_max_uv.y), 1.0f, g_camera_info.inverse_proj);
        float3 bottom_left = view_space_position_from_depth(
            float2(tile_min_uv.x, tile_min_uv.y), 1.0f, g_camera_info.inverse_proj);
        float3 bottom_right = view_space_position_from_depth(
            float2(tile_max_uv.x, tile_min_uv.y), 1.0f, g_camera_info.inverse_proj);
        float3 inside_point = view_space_position_from_depth(tile_center_uv, 1.0f, g_camera_info.inverse_proj);
        float3 origin = float3(0.0f, 0.0f, 0.0f);

        tile_frustum_planes[0] = create_plane_from_points(origin, bottom_left, top_left, inside_point);     // Left
        tile_frustum_planes[1] = create_plane_from_points(origin, top_right, bottom_right, inside_point);   // Right
        tile_frustum_planes[2] = create_plane_from_points(origin, top_left, top_right, inside_point);       // Top
        tile_frustum_planes[3] = create_plane_from_points(origin, bottom_right, bottom_left, inside_point); // Bottom
        tile_frustum_planes[4] = float4(0.0f, 0.0f, -1.0f, -min_depth);                                     // Near
        tile_frustum_planes[5] = float4(0.0f, 0.0f, 1.0f, max_depth);                                       // Far
    }

    GroupMemoryBarrierWithGroupSync();

    // 4. Cull lights

    uint32_t num_point_lights = g_point_lights.getCount();

    uint32_t thread_count = TILE_SIZE * TILE_SIZE;
    uint32_t iterations_per_thread = (num_point_lights + thread_count - 1) / thread_count;

    for (uint32_t i = 0; i < iterations_per_thread; ++i)
    {
        uint32_t light_index = i * thread_count + group_index;
        if (light_index >= num_point_lights)
            break;

        PointLight light = g_point_lights[light_index];
        float3 light_position_vs = mul(g_camera_info.view, float4(light.position, 1.0f)).xyz;

        float plane_distance = 0.0f;
        for (uint32_t j = 0; j < 6; ++j)
        {
            plane_distance = dot(float4(light_position_vs, 1.0f), tile_frustum_planes[j]) + light.radius;
            if (plane_distance <= 0.0f)
                break;
        }

        if (plane_distance > 0.0f)
        {
            uint32_t original_value;
            InterlockedAdd(tile_visible_light_count, 1, original_value);
            tile_visible_lights[original_value] = encode_tile_visible_light(LIGHT_TYPE_POINT, (uint16_t)light_index);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // 5. Copy the tile visible lights into the global array

    if (group_index == 0)
    {
        uint32_t index = group_id.y * uint32_t(g_light_culling_info.num_tiles.x) + group_id.x;

        uint32_t offset = index * MAX_LIGHTS_PER_TILE;
        for (uint32_t i = 0; i < tile_visible_light_count; ++i)
        {
            g_tile_visible_lights[offset + i] = tile_visible_lights[i];
        }

        if (tile_visible_light_count != MAX_LIGHTS_PER_TILE)
        {
            g_tile_visible_lights[offset + tile_visible_light_count] = INVALID_TILE_VISIBLE_LIGHT;
        }
    }
}
