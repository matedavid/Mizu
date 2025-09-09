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
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

#include "common/FrameInfo.slang"
#include "common/MizuBase.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

[[vk::binding(0, 1)]]
StructuredBuffer<float> cascadeSplits : register(t0, space1);
[[vk::binding(1, 1)]]
Texture2D<float> depthTexture : register(t1, space1);
[[vk::binding(2, 1)]]
SamplerState sampler : register(s0, space1);

static const uint32_t MAX_CASCADE_COLORS = 4;
static const float3 cascadeColors[MAX_CASCADE_COLORS] = {
    float3(1.0f, 0.0f, 0.0f),
    float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.0f, 1.0f),
    float3(0.75f, 0.25f, 0.25f),
};

[shader("fragment")]
float4 fsCascadesMain(VertexOutput input)
{
    uint32_t numCascades = cascadeSplits.getCount();
    float depth = depthTexture.Sample(sampler, input.texCoord);

    float3 positionVS = viewSpacePositionFromDepth(input.texCoord, depth, cameraInfo.inverseProj);

    uint32_t cascadeIdx = 0;
    for (uint32_t i = 0; i < numCascades - 1; ++i)
    {
        if (positionVS.z < cascadeSplits[i])
        {
            cascadeIdx = i + 1;
        }
    }

    if (cascadeIdx >= MAX_CASCADE_COLORS)
        cascadeIdx = MAX_CASCADE_COLORS - 1;

    float3 color = cascadeColors[cascadeIdx];
    return float4(color, 0.1f);
}

[[vk::binding(3, 1)]]
Texture2D<float> shadowMapTexture : register(t2, space1);

[shader("fragment")]
float4 fsTextureMain(VertexOutput input)
{
    float2 texCoord = float2(input.texCoord.x, 1.0f - input.texCoord.y);
    return shadowMapTexture.Sample(sampler, texCoord);
}
