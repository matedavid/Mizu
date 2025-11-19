//
// Vertex
//

#include "common/FrameInfo.slang"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

struct InstanceTransformInfo
{
    float4x4 transform;
    float4x4 normal_matrix;
};

[[vk::binding(1, 0)]]
StructuredBuffer<InstanceTransformInfo> transformInfo : register(t0, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<uint64_t> transformIndices : register(t1, space0);

struct PushConstant
{
    uint64_t transformOffset;
}

[push_constant]
PushConstant pushConstant;

[shader("vertex")]
VertexOutput vsMain(VertexInput input, uint32_t instanceId: SV_InstanceID)
{
    float4 pos = float4(input.position, 1.0f);

    uint64_t transformIndex = transformIndices[pushConstant.transformOffset + instanceId];
    InstanceTransformInfo transformInfo = transformInfo[transformIndex];

    float4x4 modelMatrix = transformInfo.transform;
    float3x3 normalMatrix = float3x3(transformInfo.normal_matrix);

    VertexOutput output;
    output.position = mul(cameraInfo.viewProj, mul(modelMatrix, pos));
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

// struct FragmentOutput
// {
//     float4 normal : SV_Target0;
// }

[shader("fragment")]
void fsMain(VertexOutput input)
{
    // FragmentOutput output;
    // output.normal = float4(normalize(input.normal), 0.0f);

    // return output;
}

