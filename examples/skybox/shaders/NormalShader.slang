//
// Vertex
//

struct VertexInput {
    float3 position;
    float3 normal;
    float2 texCoord;
}

struct VertexOutput {
    float4 position : SV_Position;
    float3 normal;
    float2 texCoord;
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
VertexOutput vertexMain(VertexInput input) {
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

[shader("fragment")]
float4 fragmentMain(VertexOutput input) {
    float3 lightPos = float3(2.0, 1.0, 2.0);
    float cosTheta = clamp(dot(input.normal, lightPos), 0.3, 1.0);

    return float4(1.0, 0.0, 0.0, 1.0) * cosTheta;
}
