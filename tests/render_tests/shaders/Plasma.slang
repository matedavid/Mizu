#include "common/MizuBase.slang"

static float PI = 3.14159265;

MIZU_VK_BINDING_UAV(0, 0)
RWTexture2D<float4> uOutput : register(u0, space0);

struct PlasmaInfo
{
    uint32_t width;
    uint32_t height;
    float time;
};

[push_constant]
PlasmaInfo uPlasmaInfo;

[shader("compute")]
[numthreads(16, 16, 1)]
void csMain(uint3 threadId: SV_DispatchThreadID)
{
    float2 uv = float2(threadId.xy) / float2(uPlasmaInfo.width, uPlasmaInfo.height);

    float t = uPlasmaInfo.time * 10.0;
    const float2 vp = float2(512.0, 512.0);

    float2 p0 = (uv - 0.5) * vp;
    float2 hvp = vp * 0.5;
    float2 p1d = float2(cos(t / 98.0), sin(t / 178.0)) * hvp - p0;
    float2 p2d = float2(sin(-t / 124.0), cos(-t / 104.0)) * hvp - p0;
    float2 p3d = float2(cos(-t / 165.0), cos(t / 45.0)) * hvp - p0;
    float sum = 0.5
                + 0.5
                      * (cos(length(p1d) / 30.0) + cos(length(p2d) / 20.0)
                         + sin(length(p3d) / 25.0) * sin(p3d.x / 20.0) * sin(p3d.y / 15.0));

    float i = fract(sum);
    float3 t2 = (64.0 * uPlasmaInfo.time) / float3(63.0, 78.0, 45.0);
    float3 cs = cos(i * PI * 2.0 + float3(0.0, 1.0, -0.5) * PI + t2);
    cs = 0.5 * cs + 0.5;

    uOutput[threadId.xy] = float4(cs, cs.x);
}

struct VertexInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.pos, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}

MIZU_VK_BINDING_SRV(0, 0)
Texture2D<float4> uTexture : register(t0, space0);

MIZU_VK_BINDING_SAMPLER(0, 0)
SamplerState uTexture_Sampler : register(s0, space0);

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target0
{
    return uTexture.Sample(uTexture_Sampler, input.texCoord);
}
