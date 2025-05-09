#include "common/MizuBase.slang"

//
// Vertex
//

struct VertexOutput
{
    float4 position : SV_Position;
    float3 texCoord;
}

struct IrradianceConvolutionShader
{
    float4x4 projection;
    float4x4 view;
}

[push_constant]
IrradianceConvolutionShader info;

[shader("vertex")]
VertexOutput vsMain(float3 position)
{
    // clang-format off
    float4x4 view = float4x4(
        info.view._11, info.view._12, info.view._13, 0.0,
        info.view._21, info.view._22, info.view._23, 0.0,
        info.view._31, info.view._32, info.view._33, 0.0,
        0.0,           0.0,           0.0,           1.0
    );
    // clang-format on

    float4 pos = mul(info.projection, mul(view, float4(position, 1.0)));

    VertexOutput output;
    output.position = pos.xyww;
    output.texCoord = position;

    return output;
}

//
// Fragment
//

[[vk::binding(0, 1)]]
TextureCube environmentMap : register(t0, space1);
[[vk::binding(1, 1)]]
SamplerState sampler : register(s0, space1);

static float PI = 3.14159265359f;

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float3 normal = normalize(input.texCoord);

    float3 irradiance = float3(0.0f);

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025f;
    float nrSamples = 0.0f;
    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += environmentMap.Sample(sampler, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0f / float(nrSamples));

    return float4(irradiance, 1.0f);
}
