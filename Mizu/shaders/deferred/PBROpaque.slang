//
// Vertex
//

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

struct CameraInfo
{
    float4x4 view;
    float4x4 projection;
}

ParameterBlock<CameraInfo> uCameraInfo;

struct ModelInfo
{
    float4x4 model;
}

[push_constant]
ModelInfo uModelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    float4 pos = float4(input.position, 1.0);

    VertexOutput output;
    output.position = mul(uCameraInfo.projection, mul(uCameraInfo.view, mul(uModelInfo.model, pos)));
    output.world_position = mul(uModelInfo.model, pos);
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

#define MIZU_COMBINED_IMAGE_SAMPLER(texType, name, set, location) \
    [[vk::binding(set, location)]]                                \
    texType name : register(t##binding, space##set);              \
    [[vk::binding(set, location)]]                                \
    SamplerState _##name##Sampler : register(s##binding, space##set)

MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, albedo, 1, 0);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, metallic, 1, 1);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, roughness, 1, 2);

struct FragmentOutput
{
    float4 albedo;
    float4 position;
    float4 normal;
    float4 metallicRoughnessAO;
}

[shader("fragment")]
FragmentOutput fsMain(VertexOutput input)
{
    float4 metallicRoughnessAO = float4(0.0f, 0.0f, 0.0f, 1.0f);
    metallicRoughnessAO.x = metallic.Sample(_metallicSampler, input.texCoord).x;
    metallicRoughnessAO.y = roughness.Sample(_roughnessSampler, input.texCoord).x;

    FragmentOutput output;
    output.albedo = albedo.Sample(_albedoSampler, input.texCoord);
    output.position = input.world_position;
    output.normal = float4(input.normal.x, input.normal.y, 0.0f, 1.0f);
    output.metallicRoughnessAO = metallicRoughnessAO;

    return output;
}

