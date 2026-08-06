#include "Base/Base.slang"
#include "Base/DescriptorBindings.slang"
#include "Base/GpuDrivenRendering.slang"
#include "Base/Utils.slang"

//
// Vertex
//

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 worldPosition : POSITION1;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;

    nointerpolation uint32_t materialOffset : TEXCOORD1;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<InstanceTransformInfo> g_transformInfo : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<GpuDrawData> g_drawData : register(t1, space0);

MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<CameraInfo> g_cameraInfo : register(b0, space1);

struct PushConstant
{
    uint32_t drawIndex;
};

[push_constant]
PushConstant pushConstant;

[shader("vertex")]
VertexOutput vsMain(VertexInput input, uint32_t instanceId: SV_InstanceID MIZU_DRAW_INDEX_PARAM)
{
    float4 pos = float4(input.position, 1.0f);

    uint32_t drawDataIndex = pushConstant.drawIndex + MIZU_GET_SV_DRAW_INDEX() + instanceId;

    GpuDrawData drawData = g_drawData[drawDataIndex];
    InstanceTransformInfo transformInfo = g_transformInfo[drawData.transformSlot];

    float4x4 modelMatrix = transformInfo.transform;
    float3x3 normalMatrix = float3x3(transformInfo.normal_matrix);

    VertexOutput output;
    output.position = mul(g_cameraInfo.viewProj, mul(modelMatrix, pos));
    output.worldPosition = mul(modelMatrix, pos);
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.texCoord = input.texCoord;
    output.materialOffset = drawData.materialOffset;

    return output;
}

//
// Fragment
//

MIZU_VK_BINDING_SRV(0, 1)
StructuredBuffer<uint32_t> g_materialBuffer : register(t0, space1);

MIZU_VK_BINDING_SAMPLER(0, 1)
SamplerState g_sampler : register(s0, space1);

MIZU_VK_BINDING_SRV(0, 2)
Texture2D<float4> g_textures[] : register(t0, space2);

struct FragmentOutput
{
    float2 gbuffer0 : SV_Target0;
    float4 gbuffer1 : SV_Target1;
    float4 gbuffer2 : SV_Target2;
};

#define MIZU_TEXTURE_OFFSET_ALBEDO 0
#define MIZU_TEXTURE_OFFSET_METALLIC 1
#define MIZU_TEXTURE_OFFSET_ROUGHNESS 2
#define MIZU_TEXTURE_OFFSET_AO 3

Texture2D<float4> getTexture(uint32_t offset, uint32_t texture)
{
    uint32_t textureIndex = g_materialBuffer[offset + texture];
    return g_textures[textureIndex];
}

[shader("fragment")]
FragmentOutput fsMain(VertexOutput input) : SV_Target0
{
    Texture2D<float4> albedoTexture = getTexture(input.materialOffset, MIZU_TEXTURE_OFFSET_ALBEDO);
    Texture2D<float4> metallicTexture = getTexture(input.materialOffset, MIZU_TEXTURE_OFFSET_METALLIC);
    Texture2D<float4> roughnessTexture = getTexture(input.materialOffset, MIZU_TEXTURE_OFFSET_ROUGHNESS);
    Texture2D<float4> aoTexture = getTexture(input.materialOffset, MIZU_TEXTURE_OFFSET_AO);

    float4 albedo = albedoTexture.Sample(g_sampler, input.texCoord);
    float metallic = metallicTexture.Sample(g_sampler, input.texCoord).r;
    float roughness = roughnessTexture.Sample(g_sampler, input.texCoord).g;
    float ao = aoTexture.Sample(g_sampler, input.texCoord).b;

    FragmentOutput output;
    output.gbuffer0 = encodeNormalSpherical(input.normal);
    output.gbuffer1 = albedo;
    output.gbuffer2 = float4(metallic, roughness, ao, 0.0f);

    return output;
}

