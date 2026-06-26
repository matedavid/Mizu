struct VertexInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.pos, 1.0f);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0f);
}
