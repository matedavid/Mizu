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
    uint32_t textureIdx : TEXCOORD1;
};

struct ConstantData
{
    float4 colorMask;
}

[[vk::binding(2, 0)]]
ConstantBuffer<ConstantData> constantBuffer : register(b0, space0);

struct PushConstantData
{
    uint32_t textureIdx;
    float3 _padding;
}

[push_constant]
PushConstantData pushConstant;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.pos, 1.0);
    output.texCoord = input.texCoord;
    output.color = input.color * constantBuffer.colorMask.rgb;
    output.textureIdx = pushConstant.textureIdx;
    return output;
}

[[vk::binding(1, 0)]]
SamplerState sampler : register(s0, space0);

[[vk::binding(3, 0)]]
StructuredBuffer<float> buffer : register(t1, space0);

[[vk::binding(4, 0)]]
ByteAddressBuffer buffer2 : register(t2, space0);

[[vk::binding(0, 1)]]
Texture2D<float4> textures[] : register(t0, space1);

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target0
{
    float bufferValue = buffer.Load(0);
    uint32_t buffer2Value = buffer2.Load(0);

    float4 textureColor = textures[input.textureIdx].Sample(sampler, input.texCoord);
    float4 color =
        float4(input.color, 1.0f) * float4(textureColor.rgb, 1.0f) * float4(bufferValue) * float4((float)buffer2Value);
    return color;
}
