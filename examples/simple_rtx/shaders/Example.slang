
// RaytracingAccelerationStructure gScene : register(t0, space0);
RWTexture2D<float4> gImage : register(t1, space0);

struct RayPayload
{
    float4 color;
};

[shader("raygen")]
void rtxRaygen()
{
    uint2 index = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    /*
    float2 uv = (float2(index) + 0.5f) / float2(dim);
    float3 origin = float3(0.0, 0.0, -5.0);
    float3 direction = normalize(float3(uv * 2.0f - 1.0f, 1.0f));

    // Setup ray
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.0f;
    ray.TMax = 10000.0f;

    // Setup payload
    RayPayload payload;
    payload.color = float4(0.0, 0.0, 0.0, 1.0);

    TraceRay(gScene,        // Accel structure
             RAY_FLAG_NONE, // Flags
             0xFF,          // Instance mask
             0,             // Hit group offset
             1,             // Geometry multiplier
             0,             // Miss shader index
             ray,           // Ray description
             payload        // Payload
    );
    */

    gImage[index] = float4(0.5f, 0.5f, 0.5f, 1.0f);
}

[shader("miss")]
void rtxMiss(inout RayPayload payload)
{
    payload.color = float4(0.0f, 0.1f, 0.3f, 1.0f);
}

[shader("closesthit")]
void rtxClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.color = float4(1.0, 0.0, 0.0, 1.0);
}
