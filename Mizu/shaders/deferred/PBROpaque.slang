#include "common/MizuBase.slang"

//
// Vertex
//

struct CameraInfo
{
    float4x4 view;
    float4x4 projection;
    float4x4 inverseView;
    float4x4 inverseProjection;
    float3 cameraPosition;
}

[[vk::binding(1, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b1, space0);

struct VertexInput
{
    float3 position;
    float3 normal;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 world_position;
    float3 normal;
    float2 texCoord;
}

struct ModelInfo
{
    float4x4 model;
}

[push_constant]
ModelInfo modelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    float4 pos = float4(input.position, 1.0);

    // TODO: Should probably move this calculation outside of the vertex shader
    float3x3 normalMatrix = transpose(inverse(float3x3(mul(cameraInfo.view, modelInfo.model))));

    VertexOutput output;
    output.position = mul(cameraInfo.projection, mul(cameraInfo.view, mul(modelInfo.model, pos)));
    output.world_position = mul(modelInfo.model, pos);
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

[[vk::binding(0, 1)]]
Texture2D<float4> albedo : register(t0, space1);
[[vk::binding(1, 1)]]
Texture2D<float4> metallic : register(t1, space1);
[[vk::binding(2, 1)]]
Texture2D<float4> roughness : register(t2, space1);
[[vk::binding(3, 1)]]
Texture2D<float4> ambientOcclusion : register(t3, space1);
[[vk::binding(4, 1)]]
SamplerState sampler : register(s0, space1);

struct FragmentOutput
{
    float4 albedo;
    float4 normal;
    float4 metallicRoughnessAO;
}

[shader("fragment")]
FragmentOutput fsMain(VertexOutput input)
{
    float4 metallicRoughnessAO = float4(0.0f, 0.0f, 0.0f, 0.0f);
    metallicRoughnessAO.x = metallic.Sample(sampler, input.texCoord).r;
    metallicRoughnessAO.y = roughness.Sample(sampler, input.texCoord).g;
    metallicRoughnessAO.z = ambientOcclusion.Sample(sampler, input.texCoord).b;

    FragmentOutput output;
    output.albedo = albedo.Sample(sampler, input.texCoord);
    output.normal = float4(input.normal, 0.0f);
    output.metallicRoughnessAO = metallicRoughnessAO;

    return output;
}

