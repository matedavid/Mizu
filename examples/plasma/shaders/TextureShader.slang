#define MIZU_SRV_BINDING_OFFSET 0
#define MIZU_UAV_BINDING_OFFSET 16
#define MIZU_CBV_BINDING_OFFSET 32
#define MIZU_SAMPLER_BINDING_OFFSET 64

#define MIZU_VK_BINDING_SRV(_binding, _set) [[vk::binding(_binding + MIZU_SRV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_UAV(_binding, _set) [[vk::binding(_binding + MIZU_UAV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_CBV(_binding, _set) [[vk::binding(_binding + MIZU_CBV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_SAMPLER(_binding, _set) [[vk::binding(_binding + MIZU_SAMPLER_BINDING_OFFSET, _set)]]

//
// Vertex
//

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct CameraInfo
{
    float4x4 view;
    float4x4 projection;
};

MIZU_VK_BINDING_CBV(0, 0)
ConstantBuffer<CameraInfo> uCameraInfo : register(b0, space0);

struct ModelInfoType
{
    float4x4 model;
};

[push_constant]
ModelInfoType uModelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position =
        mul(uCameraInfo.projection, mul(uCameraInfo.view, mul(uModelInfo.model, float4(input.position, 1.0))));
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

MIZU_VK_BINDING_SRV(0, 0)
Texture2D<float4> uTexture : register(t0, space0);
MIZU_VK_BINDING_SAMPLER(0, 0)
SamplerState uTexture_Sampler : register(s0, space0);

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target0
{
    return uTexture.Sample(uTexture_Sampler, input.texCoord);
}
