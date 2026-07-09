#include "Base/Base.slang"
#include "Base/DescriptorBindings.slang"

//
// Vertex
//

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct InstanceTransformInfo
{
    float4x4 transform;
    float4x4 normal_matrix;
};

MIZU_VK_BINDING_SRV(0, 0)
StructuredBuffer<InstanceTransformInfo> g_transformInfo : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
StructuredBuffer<uint32_t> g_viewIndices : register(t1, space0);

MIZU_VK_BINDING_CBV(0, 1)
ConstantBuffer<CameraInfo> g_cameraInfo : register(b0, space1);

struct PushConstant
{
    uint32_t viewIndicesOffset;
}

[push_constant]
PushConstant pushConstant;

[shader("vertex")]
VertexOutput vsMain(VertexInput input, uint32_t instanceId: SV_InstanceID)
{
    float4 pos = float4(input.position, 1.0f);

    uint64_t transformIndex = g_viewIndices[pushConstant.viewIndicesOffset + instanceId];
    InstanceTransformInfo transformInfo = g_transformInfo[transformIndex];

    float4x4 modelMatrix = transformInfo.transform;
    float3x3 normalMatrix = float3x3(transformInfo.normal_matrix);

    VertexOutput output;
    output.position = mul(g_cameraInfo.viewProj, mul(modelMatrix, pos));
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

