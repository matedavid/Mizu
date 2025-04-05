#include "common/MizuBase.slang"

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
RWTexture2D<float> mainOutput : register(t5, space1);

struct SSAOMainInfo
{
    uint32_t width, height;
    float radius;
    float bias;
}

[push_constant]
SSAOMainInfo ssaoMainInfo;

[shader("compute")]
[numthreads(16, 16, 1)]
void ssaoMain(uint3 threadId: SV_DispatchThreadID)
{
    if (threadId.x >= ssaoMainInfo.width || threadId.y >= ssaoMainInfo.height)
    {
        return;
    }

    float2 texCoords =
        float2(float(threadId.x) / float(ssaoMainInfo.width), float(threadId.y) / float(ssaoMainInfo.height));

    float3 position = viewSpacePositionFromDepth(
        texCoords, gDepth.SampleLevel(sampler, texCoords, 0), cameraInfo.inverseProjection);
    float3 normal = normalize(gNormal.SampleLevel(sampler, texCoords, 0).xyz * 2.0f - 1.0f);

    uint2 noiseDim;
    ssaoNoise.GetDimensions(noiseDim.x, noiseDim.y);

    float2 noiseCoords =
        float2(float(ssaoMainInfo.width) / float(noiseDim.x), float(ssaoMainInfo.height) / (noiseDim.y)) * texCoords;
    float3 randomVec = ssaoNoise.SampleLevel(sampler, noiseCoords, 0).xyz;

    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(tangent, normal);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    for (uint32_t i = 0; i < ssaoKernel.getCount(); ++i)
    {
        float3 samplePos = mul(TBN, ssaoKernel[i]);
        samplePos = position + samplePos * ssaoMainInfo.radius;

        float4 offset = float4(samplePos, 1.0f);
        offset = mul(cameraInfo.projection, offset);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;

        float sampleDepth = gDepth.SampleLevel(sampler, offset.xy, 0);

        // float rangeCheck = smoothstep(0.0f, 1.0f, ssaoInfo.radius / abs(position.z - sampleDepth));
        occlusion += (sampleDepth <= samplePos.z - ssaoMainInfo.bias ? 1.0f : 0.0f); // * rangeCheck;
    }
    occlusion = 1.0f - (occlusion / float(ssaoKernel.getCount()));

    mainOutput[threadId.xy] = occlusion;
}

[[vk::binding(0, 1)]]
Texture2D<float> blurInput : register(t0, space1);
[[vk::binding(1, 1)]]
RWTexture2D<float> blurOutput : register(t1, space1);
[[vk::binding(2, 1)]]
SamplerState blurSampler : register(t2, space1);

struct SSAOBlurInfo
{
    uint32_t width, height;
    uint32_t range;
}

[push_constant]
SSAOBlurInfo ssaoBlurInfo;

[shader("compute")]
[numthreads(16, 16, 1)]
void ssaoBlur(uint3 threadId: SV_DispatchThreadID)
{
    if (threadId.x >= ssaoBlurInfo.width || threadId.y >= ssaoBlurInfo.height)
    {
        return;
    }

    float2 texCoords =
        float2(float(threadId.x) / float(ssaoBlurInfo.width), float(threadId.y) / float(ssaoBlurInfo.height));

    float2 texelSize = 1.0f / float2(float(ssaoBlurInfo.width), float(ssaoBlurInfo.height));
    int32_t irange = int32_t(ssaoBlurInfo.range);

    float result = 0.0f;
    for (int32_t x = -irange; x <= irange; ++x)
    {
        for (int32_t y = -irange; y <= irange; ++y)
        {
            float2 offset = float2(float(x), float(y)) * texelSize;
            result += blurInput.SampleLevel(blurSampler, texCoords + offset, 0);
        }
    }

    uint32_t iters = ssaoBlurInfo.range * 2 + 1;

    // blurOutput[threadId.xy] = result / float(iters * iters);
    blurOutput[threadId.xy] = 1.0f + min(result, 0.0f);
}
