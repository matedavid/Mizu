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

MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, uPresentTexture, 0, 0);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    return uPresentTexture.Sample(_uPresentTextureSampler, input.texCoord);
}

