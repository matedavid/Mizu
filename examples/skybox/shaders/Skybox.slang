//
// Vertex
//

struct VertexOutput {
    float4 position : SV_Position;
    float3 texCoord;
}

struct CameraInfo {
    float4x4 view;
    float4x4 projection;
}

ParameterBlock<CameraInfo> uCameraInfo;

struct ModelInfoType {
    float4x4 model;
}

[push_constant]
ModelInfoType uModelInfo;

[shader("vertex")]
VertexOutput vertexMain(float3 position) {
    float4x4 view = float4x4(
        uCameraInfo.view._11, uCameraInfo.view._12, uCameraInfo.view._13, 0.0,
        uCameraInfo.view._21, uCameraInfo.view._22, uCameraInfo.view._23, 0.0,
        uCameraInfo.view._31, uCameraInfo.view._32, uCameraInfo.view._33, 0.0,
        0.0,                  0.0,                  0.0,                  1.0
    );

    float4 pos = mul(uCameraInfo.projection, mul(view, mul(uModelInfo.model, float4(position, 1.0))));

    VertexOutput output;
    output.position = pos.xyww;
    output.texCoord = position;

    return output;
}

//
// Fragment
//

TextureCube uSkybox : register(t0, space1);
SamplerState uSkybox_Sampler : register(s0, space1);

[shader("fragment")]
float4 fragmentMain(VertexOutput input) {
    float3 color = uSkybox.Sample(uSkybox_Sampler, input.texCoord).rgb;
    return float4(color, 1.0);
}
