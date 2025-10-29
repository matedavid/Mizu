//
// Vertex
//

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
}

struct CameraInfo
{
    float4x4 view;
    float4x4 projection;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> uCameraInfo : register(b0, space0);

struct ModelInfoType
{
    float4x4 model;
}

[push_constant]
ModelInfoType uModelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    VertexOutput output;
    output.position =
        mul(uCameraInfo.projection, mul(uCameraInfo.view, mul(uModelInfo.model, float4(input.position, 1.0))));
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

[[vk::binding(0, 1)]]
Texture2D<float4> uTexture : register(t0, space1);
[[vk::binding(1, 1)]]
SamplerState uTexture_Sampler : register(s0, space1);

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target0
{
    float3 lightPos = float3(2.0, 1.0, 2.0);
    float cosTheta = clamp(dot(input.normal, lightPos), 0.3, 1.0);

    float4 color = uTexture.Sample(uTexture_Sampler, input.texCoord);
    return color * cosTheta;
}
