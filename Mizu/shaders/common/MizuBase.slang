float3 viewSpacePositionFromDepth(float2 uv, float depth, float4x4 inverseProjection)
{
    float x = uv.x * 2.0 - 1.0;
    float y = uv.y * 2.0 - 1.0;

    float4 projectedPos = float4(x, y, depth, 1.0f);
    float4 viewSpacePosition = mul(inverseProjection, projectedPos);

    return viewSpacePosition.xyz / viewSpacePosition.w;
}
