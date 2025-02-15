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

ConstantBuffer<CameraInfo> cameraInfo : register(b1, space0);

struct VertexInput
{
    float3 position;
    float3 normal;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 world_position;
    float3 normal;
    float2 texCoord;
}

struct ModelInfo
{
    float4x4 model;
}

[push_constant]
ModelInfo uModelInfo;

float3x3 inverse(float3x3 m)
{
    float det = m._11 * (m._22 * m._33 - m._23 * m._32) - m._12 * (m._21 * m._33 - m._23 * m._31)
                + m._13 * (m._21 * m._32 - m._22 * m._31);

    if (det == 0.0)
    {
        return float3x3(0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    float invDet = 1.0 / det;

    float3x3 adjugate;
    adjugate._11 = (m._22 * m._33 - m._23 * m._32);
    adjugate._12 = -(m._12 * m._33 - m._13 * m._32);
    adjugate._13 = (m._12 * m._23 - m._13 * m._22);

    adjugate._21 = -(m._21 * m._33 - m._23 * m._31);
    adjugate._22 = (m._11 * m._33 - m._13 * m._31);
    adjugate._23 = -(m._11 * m._23 - m._13 * m._21);

    adjugate._31 = (m._21 * m._32 - m._22 * m._31);
    adjugate._32 = -(m._11 * m._32 - m._12 * m._31);
    adjugate._33 = (m._11 * m._22 - m._12 * m._21);

    return adjugate * invDet;
}

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    float4 pos = float4(input.position, 1.0);
    // TODO: Should probably move this calculation outside of the vertex shader
    float3x3 normalMatrix = transpose(inverse(float3x3(uModelInfo.model)));

    VertexOutput output;
    output.position = mul(cameraInfo.projection, mul(cameraInfo.view, mul(uModelInfo.model, pos)));
    output.world_position = mul(uModelInfo.model, pos);
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, albedo, 1, 0);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, metallic, 1, 1);
MIZU_COMBINED_IMAGE_SAMPLER(Texture2D<float4>, roughness, 1, 2);

struct FragmentOutput
{
    float4 albedo;
    float4 position;
    float4 normal;
    float4 metallicRoughnessAO;
}

[shader("fragment")]
FragmentOutput fsMain(VertexOutput input)
{
    float4 metallicRoughnessAO = float4(0.0f, 0.0f, 0.0f, 0.0f);
    metallicRoughnessAO.x = metallic.Sample(_metallicSampler, input.texCoord).x;
    metallicRoughnessAO.y = roughness.Sample(_roughnessSampler, input.texCoord).x;

    FragmentOutput output;
    output.albedo = albedo.Sample(_albedoSampler, input.texCoord);
    output.position = input.world_position;
    output.normal = float4(input.normal.x, input.normal.y, input.normal.z, 0.0f);
    output.metallicRoughnessAO = metallicRoughnessAO;

    return output;
}

