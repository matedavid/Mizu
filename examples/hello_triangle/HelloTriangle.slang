#define MIZU_SRV_BINDING_OFFSET 0
#define MIZU_UAV_BINDING_OFFSET 16
#define MIZU_CBV_BINDING_OFFSET 32
#define MIZU_SAMPLER_BINDING_OFFSET 64

#define MIZU_VK_BINDING_SRV(_binding, _set) [[vk::binding(_binding + MIZU_SRV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_UAV(_binding, _set) [[vk::binding(_binding + MIZU_UAV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_CBV(_binding, _set) [[vk::binding(_binding + MIZU_CBV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_SAMPLER(_binding, _set) [[vk::binding(_binding + MIZU_SAMPLER_BINDING_OFFSET, _set)]]

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
};

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<ConstantData> constantBuffer : register(b0, space0);

struct PushConstantData
{
    uint32_t textureIdx;
    float3 _padding;
};

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

MIZU_VK_BINDING_SAMPLER(0, 0)
SamplerState sampler : register(s0, space0);

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<float> buffer : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
ByteAddressBuffer buffer2 : register(t1, space0);

MIZU_VK_BINDING_SRV(0, 1)
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
