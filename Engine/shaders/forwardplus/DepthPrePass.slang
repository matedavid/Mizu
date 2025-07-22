//
// Vertex
//

#include "common/FrameInfo.slang"

struct VertexInput
{
    float3 position;
    float3 normal;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal;
    float2 texCoord;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

struct ModelInfo
{
    float4x4 model;
    float4x4 normalMatrix;
}

[push_constant]
ModelInfo modelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    float4 pos = float4(input.position, 1.0f);
    float3x3 normalMatrix = float3x3(modelInfo.normalMatrix);

    VertexOutput output;
    output.position = mul(cameraInfo.proj, mul(cameraInfo.view, mul(modelInfo.model, pos)));
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

[shader("fragment")]
void fsMain(VertexOutput input) {}

