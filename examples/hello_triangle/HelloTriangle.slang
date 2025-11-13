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

struct ConstantData
{
    float4 colorMask;
}

ConstantBuffer<ConstantData> constantBuffer : register(b0, space0);

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.pos, 1.0);
    output.texCoord = input.texCoord;
    output.color = input.color * constantBuffer.colorMask.rgb;
    return output;
}

[[vk::binding(0, 0)]]
Texture2D<float4> texture : register(t0, space0);

[[vk::binding(1, 0)]]
SamplerState sampler : register(s0, space0);

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target0
{
    float4 textureColor = texture.Sample(sampler, input.texCoord);
    float4 color = float4(input.color, 1.0f) * float4(textureColor.rgb, 1.0f);
    return color;
}
