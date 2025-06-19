

struct CameraInfo
{
    float4x4 viewProj;
    float4x4 viewInverse;
    float4x4 projInverse;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);
[[vk::binding(1, 0)]]
RWTexture2D<float4> output : register(t1, space0);
[[vk::binding(2, 0)]]
RaytracingAccelerationStructure scene : register(t2, space0);

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
    float2 d = inUV * 2.0f - 1.0f;

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

    TraceRay(scene,                 // Accel structure
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

[shader("miss")]
void rtxShadowMiss(inout bool isShadowed)
{
    isShadowed = false;
}

struct Vertex
{
    float3 position;
    float3 normal;
};

[[vk::binding(0, 1)]]
StructuredBuffer<Vertex> vertices : register(t0, space1);
[[vk::binding(1, 1)]]
StructuredBuffer<uint32_t> indices : register(t1, space1);

[shader("closesthit")]
void rtxClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint32_t primitiveIndex = PrimitiveIndex();
    uint3 index = indices.Load(primitiveIndex * 3);

    Vertex v0 = vertices.Load(index.x + 0);
    Vertex v1 = vertices.Load(index.y + 1);
    Vertex v2 = vertices.Load(index.z + 2);

    float3 barycentrics =
        float3(1.0f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    float3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    normal = normalize(normal);

    float3 lightDirection = normalize(float3(1.0f, -1.0f, 0.0f));
    float3 L = -lightDirection;

    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float3 baseColor = float3(0.8f, 0.8f, 0.8f);

    float NdotL = max(dot(normal, L), 0.0f);
    float3 diffuse = lightColor * baseColor * NdotL;
    float3 ambient = baseColor * 0.1f;

    if (NdotL > 0)
    {
        float3 worldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

        RayDesc shadowRay;
        shadowRay.Origin = worldPosition;
        shadowRay.Direction = L;
        shadowRay.TMin = 0.001f;
        shadowRay.TMax = 10000.0f;

        bool isShadowed = true;

        uint flags =
            RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

        TraceRay(scene,     // Accel structure
                 flags,     // Flags
                 0xFF,      // Instance mask
                 0,         // Hit group offset
                 1,         // Geometry multiplier
                 1,         // Miss shader index
                 shadowRay, // Ray description
                 isShadowed // Payload
        );

        if (isShadowed)
        {
            diffuse *= 0.3f;
        }
    }

    payload.color = float4(ambient + diffuse, 1.0f);
}
