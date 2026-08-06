#include "base/descriptor_bindings.slang"

struct CameraInfo
{
    float4x4 view_proj;
    float4x4 view_inverse;
    float4x4 proj_inverse;
};

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<CameraInfo> g_camera_info : register(b0, space0);
MIZU_VK_BINDING_UAV(0, 0)
RWTexture2D<float4> g_output : register(u0, space0);
MIZU_VK_BINDING_SRV(0, 0)
RaytracingAccelerationStructure g_scene : register(t0, space0);

struct RayPayload
{
    float4 color;
};

[shader("raygen")]
void rtx_raygen()
{
    uint3 dispatch_ray_index = DispatchRaysIndex();
    uint3 dimensions = DispatchRaysDimensions();

    float2 pixel_center = float2((float)dispatch_ray_index.x, (float)dispatch_ray_index.y);
    float2 in_uv = pixel_center / float2((float)dimensions.x, (float)dimensions.y);

    float2 d = float2(in_uv.x * 2.0f - 1.0f, 1.0f - in_uv.y * 2.0f);

    float4 origin = mul(g_camera_info.view_inverse, float4(0.0f, 0.0f, 0.0f, 1.0f));
    float4 target = mul(g_camera_info.proj_inverse, float4(d.x, d.y, 1.0f, 1.0f));
    float4 direction = mul(g_camera_info.view_inverse, float4(normalize(target.xyz), 0.0f));

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    RayPayload payload;
    payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    TraceRay(
        g_scene,               // Accel structure
        RAY_FLAG_FORCE_OPAQUE, // Flags
        0xFF,                  // Instance mask
        0,                     // Hit group offset
        1,                     // Geometry multiplier
        0,                     // Miss shader index
        ray,                   // Ray description
        payload                // Payload
    );

    g_output[dispatch_ray_index.xy] = payload.color;
}

[shader("miss")]
void rtx_miss(inout RayPayload payload)
{
    payload.color = float4(0.0f, 0.1f, 0.3f, 1.0f);
}

struct ShadowPayload
{
    bool is_shadowed;
};

[shader("miss")]
void rtx_shadow_miss(inout ShadowPayload payload)
{
    payload.is_shadowed = false;
}

struct Vertex
{
    float3 position;
    float3 normal;
};

struct PointLight
{
    float3 position;
    float radius;
    float4 color;
};

MIZU_VK_BINDING_SRV(0, 1)
StructuredBuffer<Vertex> g_vertices : register(t0, space1);
MIZU_VK_BINDING_SRV(1, 1)
StructuredBuffer<uint32_t> g_indices : register(t1, space1);
MIZU_VK_BINDING_SRV(2, 1)
StructuredBuffer<PointLight> g_point_lights : register(t2, space1);

[shader("closesthit")]
void rtx_closest_hit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint32_t primitive_index = PrimitiveIndex();

    uint i0 = g_indices[primitive_index * 3 + 0];
    uint i1 = g_indices[primitive_index * 3 + 1];
    uint i2 = g_indices[primitive_index * 3 + 2];

    Vertex v0 = g_vertices[i0];
    Vertex v1 = g_vertices[i1];
    Vertex v2 = g_vertices[i2];

    float3 barycentrics =
        float3(1.0f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    float3 world_position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    normal = normalize(normal);

    float3 base_color = float3(1.0f);

    float4 lo = float4(0.0f);
    for (uint32_t i = 0; i < g_point_lights.getCount(); ++i)
    {
        PointLight light = g_point_lights[i];

        float3 l = normalize(light.position - world_position);
        float n_dot_l = max(dot(normal, l), 0.0f);

        float3 diffuse = light.color.rgb * base_color * n_dot_l;
        float3 ambient = base_color * 0.1f;

        if (n_dot_l > 0)
        {
            RayDesc shadow_ray;
            shadow_ray.Origin = world_position;
            shadow_ray.Direction = l;
            shadow_ray.TMin = 0.001f;
            shadow_ray.TMax = distance(world_position, light.position);

            uint flags =
                RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

            ShadowPayload payload;
            payload.is_shadowed = true;

            TraceRay(
                g_scene,    // Accel structure
                flags,      // Flags
                0xFF,       // Instance mask
                0,          // Hit group offset
                1,          // Geometry multiplier
                1,          // Miss shader index
                shadow_ray, // Ray description
                payload     // Payload
            );

            if (payload.is_shadowed)
            {
                diffuse *= 0.3f;
            }
        }

        lo += float4(diffuse + ambient, 1.0f);
    }

    payload.color = lo;
}
