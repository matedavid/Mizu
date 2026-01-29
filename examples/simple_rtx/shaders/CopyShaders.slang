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

MIZU_VK_BINDING_SRV(0, 0)
Texture2D<float4> inputTexture : register(t0, space0);
MIZU_VK_BINDING_SAMPLER(0, 0)
SamplerState sampler : register(s0, space0);

[shader("fragment")]
float4 copyTextureFragment(VertexOutput input) : SV_Target0
{
    return inputTexture.Sample(sampler, input.texCoord);
}
