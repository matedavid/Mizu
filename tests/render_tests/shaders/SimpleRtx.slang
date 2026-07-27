#include "Base/DescriptorBindings.slang"

struct CameraInfo
{
    float4x4 viewProj;
    float4x4 viewInverse;
    float4x4 projInverse;
};

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);
MIZU_VK_BINDING_UAV(0, 0)
RWTexture2D<float4> output : register(u0, space0);
MIZU_VK_BINDING_SRV(0, 0)
RaytracingAccelerationStructure scene : register(t0, space0);

struct RayPayload
{
    float4 color;
};

[shader("raygen")]
void rtxRaygen()
{
    uint3 dispatchRayIndex = DispatchRaysIndex();
    uint3 dimensions = DispatchRaysDimensions();

    float2 pixelCenter = float2((float)dispatchRayIndex.x, (float)dispatchRayIndex.y);
    float2 inUV = pixelCenter / float2((float)dimensions.x, (float)dimensions.y);

    float2 d = float2(inUV.x * 2.0f - 1.0f, 1.0f - inUV.y * 2.0f);

    float4 origin = mul(cameraInfo.viewInverse, float4(0.0f, 0.0f, 0.0f, 1.0f));
    float4 target = mul(cameraInfo.projInverse, float4(d.x, d.y, 1.0f, 1.0f));
    float4 direction = mul(cameraInfo.viewInverse, float4(normalize(target.xyz), 0.0f));

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    RayPayload payload;
    payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    TraceRay(
        scene,                 // Accel structure
        RAY_FLAG_FORCE_OPAQUE, // Flags
        0xFF,                  // Instance mask
        0,                     // Hit group offset
        1,                     // Geometry multiplier
        0,                     // Miss shader index
        ray,                   // Ray description
        payload                // Payload
    );

    output[dispatchRayIndex.xy] = payload.color;
}

[shader("miss")]
void rtxMiss(inout RayPayload payload)
{
    payload.color = float4(0.0f, 0.1f, 0.3f, 1.0f);
}

struct ShadowPayload
{
    bool isShadowed;
};

[shader("miss")]
void rtxShadowMiss(inout ShadowPayload payload)
{
    payload.isShadowed = false;
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
StructuredBuffer<Vertex> vertices : register(t0, space1);
MIZU_VK_BINDING_SRV(1, 1)
StructuredBuffer<uint32_t> indices : register(t1, space1);
MIZU_VK_BINDING_SRV(2, 1)
StructuredBuffer<PointLight> pointLights : register(t2, space1);

[shader("closesthit")]
void rtxClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint32_t primitiveIndex = PrimitiveIndex();

    uint i0 = indices[primitiveIndex * 3 + 0];
    uint i1 = indices[primitiveIndex * 3 + 1];
    uint i2 = indices[primitiveIndex * 3 + 2];

    Vertex v0 = vertices[i0];
    Vertex v1 = vertices[i1];
    Vertex v2 = vertices[i2];

    float3 barycentrics =
        float3(1.0f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    float3 worldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    normal = normalize(normal);

    float3 baseColor = float3(1.0f);

    float4 Lo = float4(0.0f);
    for (uint32_t i = 0; i < pointLights.getCount(); ++i)
    {
        PointLight light = pointLights[i];

        float3 L = normalize(light.position - worldPosition);
        float NdotL = max(dot(normal, L), 0.0f);

        float3 diffuse = light.color.rgb * baseColor * NdotL;
        float3 ambient = baseColor * 0.1f;

        if (NdotL > 0)
        {
            RayDesc shadowRay;
            shadowRay.Origin = worldPosition;
            shadowRay.Direction = L;
            shadowRay.TMin = 0.001f;
            shadowRay.TMax = distance(worldPosition, light.position);

            uint flags =
                RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

            ShadowPayload payload;
            payload.isShadowed = true;

            TraceRay(
                scene,     // Accel structure
                flags,     // Flags
                0xFF,      // Instance mask
                0,         // Hit group offset
                1,         // Geometry multiplier
                1,         // Miss shader index
                shadowRay, // Ray description
                payload    // Payload
            );

            if (payload.isShadowed)
            {
                diffuse *= 0.3f;
            }
        }

        Lo += float4(diffuse + ambient, 1.0f);
    }

    payload.color = Lo;
}
