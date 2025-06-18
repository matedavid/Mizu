

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

[shader("closesthit")]
void rtxClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint32_t instanceIndex = InstanceIndex();

    if (instanceIndex == 0)
    {
        payload.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    else if (instanceIndex == 1)
    {
        payload.color = float4(0.0f, 1.0f, 0.0f, 1.0f);
    }
    else
    {
        payload.color = float4(0.0f, 0.0f, 1.0f, 1.0f);
    }
}
