#include "common/MizuBase.slang"

//
// Vertex
//

struct VertexOutput
{
    float4 position : SV_Position;
    float3 texCoord;
}

struct PrefilterConvolutionShader
{
    float4x4 viewProjection;
    float roughness;
}

[push_constant]
PrefilterConvolutionShader info;

[shader("vertex")]
VertexOutput vsMain(float3 position)
{
    float4 pos = mul(info.viewProjection, float4(position, 1.0));

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

float radicalInverse_VdC(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), radicalInverse_VdC(i));
}

float3 importanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float3 N = normalize(input.texCoord);
    float3 R = N;
    float3 V = R;

    const uint32_t SAMPLE_COUNT = 1024u;

    float totalWeight = 0.0f;
    float3 prefilteredColor = float3(0.0f);
    for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = hammersley(i, SAMPLE_COUNT);
        float3 H = importanceSampleGGX(Xi, N, info.roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            prefilteredColor += environmentMap.Sample(sampler, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    return float4(prefilteredColor, 1.0f);
}
