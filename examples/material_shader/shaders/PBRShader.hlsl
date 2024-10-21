//
// VertexShader
//

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

cbuffer CameraInfo : register(b0, space0)
{
    float4x4 view;
    float4x4 projection;
};

[[vk::push_constant]]
struct ModelInfoType
{
    float4x4 model;
} ModelInfo;

VSOutput VS_Main(VSInput input)
{
    VSOutput output;
    output.position = mul(projection, mul(view, mul(ModelInfo.model, float4(input.position, 1.0))));
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    
    return output;
}


//
// FragmentShader
//

Texture2D<float4> Albedo : register(t0, space1);
SamplerState Albedo_SamplerState : register(s0, space1);

float4 FS_Main(VSOutput input) : SV_Target0
{
    return Albedo.Sample(Albedo_SamplerState, input.texCoord);
}

