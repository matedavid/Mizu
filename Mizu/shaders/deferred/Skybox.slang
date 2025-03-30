#include "common/MizuBase.slang"

//
// Vertex
//

struct CameraInfo
{
    float4x4 view;
    float4x4 projection;
    float3 cameraPosition;
}

[[vk::binding(1, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b1, space0);

struct VertexOutput
{
    float4 position : SV_Position;
    float3 texCoord;
}

struct ModelInfoType
{
    float4x4 model;
}

[push_constant]
ModelInfoType modelInfo;

[shader("vertex")]
VertexOutput vsMain(float3 position)
{
    // clang-format off
    float4x4 view = float4x4(
        cameraInfo.view._11, cameraInfo.view._12, cameraInfo.view._13, 0.0,
        cameraInfo.view._21, cameraInfo.view._22, cameraInfo.view._23, 0.0,
        cameraInfo.view._31, cameraInfo.view._32, cameraInfo.view._33, 0.0,
        0.0,                  0.0,                  0.0,                  1.0
    );
    // clang-format on

    float4 pos = mul(cameraInfo.projection, mul(view, mul(modelInfo.model, float4(position, 1.0))));

    VertexOutput output;
    output.position = pos.xyww;
    output.texCoord = position;

    return output;
}

//
// Fragment
//

[[vk::binding(0, 1)]]
TextureCube skybox : register(t0, space1);
[[vk::binding(1, 1)]]
SamplerState sampler : register(s0, space1);

[shader("fragment")]
float4 fsMain(VertexOutput input)
{
    float3 color = skybox.Sample(sampler, input.texCoord).rgb;
    return float4(color, 1.0);
}
