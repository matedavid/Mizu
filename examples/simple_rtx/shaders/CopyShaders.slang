struct VertexInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput copyTextureVertex(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;

    return output;
}

[[vk::binding(0, 0)]]
Texture2D<float4> inputTexture : register(t0, space0);
[[vk::binding(1, 0)]]
SamplerState sampler : register(s0, space0);

[shader("fragment")]
float4 copyTextureFragment(VertexOutput input) : SV_Target0
{
    return inputTexture.Sample(sampler, input.texCoord);
}
