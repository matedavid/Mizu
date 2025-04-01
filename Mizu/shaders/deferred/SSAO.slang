#include "common/MizuBase.slang"

/*
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
*/

[[vk::binding(0, 1)]]
StructuredBuffer<float3> ssaoKernel : register(t0, space1);
[[vk::binding(1, 1)]]
Texture2D<float4> ssaoNoise : register(t1, space1);
[[vk::binding(2, 1)]]
Texture2D<float> gDepth : register(t2, space1);
[[vk::binding(3, 1)]]
Texture2D<float4> gNormal : register(t3, space1);
[[vk::binding(4, 1)]]
SamplerState sampler : register(t4, space1);

[[vk::binding(5, 1)]]
RWTexture2D<float> output : register(t5, space1);

struct SSAOInfo
{
    uint32_t width, height;
    float radius;
    float bias;

    float4x4 projection, inverseProjection;
}

[push_constant]
SSAOInfo ssaoInfo;

[shader("compute")]
[numthreads(16, 16, 1)]
void ssaoMain(uint3 threadId: SV_DispatchThreadID)
{
    if (threadId.x >= ssaoInfo.width || threadId.y >= ssaoInfo.height)
    {
        return;
    }

    float2 texCoords = float2(float(threadId.x) / float(ssaoInfo.width), float(threadId.y) / float(ssaoInfo.height));

    float3 position =
        viewSpacePositionFromDepth(texCoords, gDepth.SampleLevel(sampler, texCoords, 0), ssaoInfo.inverseProjection);
    float3 normal = normalize(gNormal.SampleLevel(sampler, texCoords, 0).xyz * 2.0f - 1.0f);

    uint2 noiseDim;
    ssaoNoise.GetDimensions(noiseDim.x, noiseDim.y);

    float2 noiseCoords =
        float2(float(ssaoInfo.width) / float(noiseDim.x), float(ssaoInfo.height) / (noiseDim.y)) * texCoords;
    float3 randomVec = ssaoNoise.SampleLevel(sampler, noiseCoords, 0).xyz;

    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(tangent, normal);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    for (uint32_t i = 0; i < ssaoKernel.getCount(); ++i)
    {
        float3 samplePos = mul(TBN, ssaoKernel[i]);
        samplePos = position + samplePos * ssaoInfo.radius;

        float4 offset = float4(samplePos, 1.0f);
        offset = mul(ssaoInfo.projection, offset);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;

        float sampleDepth = gDepth.SampleLevel(sampler, offset.xy, 0);

        // float rangeCheck = smoothstep(0.0f, 1.0f, ssaoInfo.radius / abs(position.z - sampleDepth));
        occlusion += (sampleDepth <= samplePos.z - ssaoInfo.bias ? 1.0f : 0.0f); // * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(ssaoKernel.getCount()));

    output[threadId.xy] = occlusion;
}
