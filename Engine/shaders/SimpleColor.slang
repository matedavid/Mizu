//
// Vertex
//

struct CameraInfo
{
    float4x4 view;
    float4x4 proj;
    float3 pos;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

struct VertexInput
{
    float3 position;
}

struct VertexOutput
{
    float4 position : SV_Position;
}

struct ModelInfo
{
    float4x4 model;
}

[push_constant]
ModelInfo modelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    float4 pos = float4(input.position, 1.0);

    VertexOutput output;
    output.position = mul(cameraInfo.proj, mul(cameraInfo.view, mul(modelInfo.model, pos)));

    return output;
}

//
// Fragment
//

struct FragmentOutput
{
    float4 albedo;
}

[shader("fragment")]
FragmentOutput fsMain(VertexOutput input)
{
    FragmentOutput output;
    output.albedo = float4(1.0f, 0.0f, 0.0f, 1.0f);

    return output;
}

