#include "base/descriptor_bindings.slang"

struct VertexInput
{
    float3 position : POSITION;
    float2 tex_coord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.uv = input.tex_coord;
    return output;
}

MIZU_VK_BINDING_SRV(0, 0)
Texture2D<float4> g_input : register(t0, space0);

MIZU_VK_BINDING_SAMPLER(0, 0)
SamplerState g_sampler : register(s0, space0);

float3 aces_tonemap(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET
{
    float3 color = g_input.Sample(g_sampler, input.uv).rgb;
    color = aces_tonemap(color);
    return float4(color, 1.0);
}
