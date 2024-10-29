struct Types 
{
    // Floats
    float f;

    float2 f2;
    float3 f3;
    float4 f4;

    float3x3 f3x3;
    float4x4 f4x4;
}

ParameterBlock<Types> types;

[shader("vertex")]
float4 vertexMain() 
{
    float rf = types.f + types.f2.x * types.f3.y + types.f4.z * types.f3x3[1][0] + types.f4x4[0][1];
    return float4(rf, rf, rf, rf);
}
