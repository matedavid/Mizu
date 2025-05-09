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

[[vk::binding(0, 0)]]
Texture2D<float4> uPresentTexture : register(t0, space0);
[[vk::binding(1, 0)]]
SamplerState uPresentTextureSampler : register(s0, space0);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    return uPresentTexture.Sample(uPresentTextureSampler, input.texCoord);
}

