//
// Vertex
//

struct VertexInput
{
    float3 position;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoord;
}

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

#define MIZU_COMBINED_IMAGE_SAMPLER(texType, name, set, _binding) \
    [[vk::binding(_binding, set)]]                                \
    texType name : register(t##_binding, space##set);             \
    [[vk::binding(_binding, set)]]                                \
    SamplerState _##name##Sampler : register(s##_binding, space##set)

MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, albedo, 1, 0);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, position, 1, 1);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, normals, 1, 2);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, metallicRoughnessAO, 1, 3);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float4 x = metallicRoughnessAO.Sample(_metallicRoughnessAOSampler, input.texCoord);
    float4 y = position.Sample(_positionSampler, input.texCoord);
    float4 z = normals.Sample(_normalsSampler, input.texCoord);
    return albedo.Sample(_albedoSampler, input.texCoord) * x.w * y.w * z.w;
}

