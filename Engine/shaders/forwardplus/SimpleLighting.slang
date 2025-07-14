//
// Vertex
//

struct VertexInput
{
    float3 position;
    float3 normal;
    float2 texCoord;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 worldPosition;
    float3 normal;
    float2 texCoord;
}

struct CameraInfo
{
    float4x4 view;
    float4x4 proj;
    float3 pos;
}

[[vk::binding(0, 0)]]
ConstantBuffer<CameraInfo> cameraInfo : register(b0, space0);

struct ModelInfo
{
    float4x4 model;
}

[push_constant]
ModelInfo modelInfo;

[shader("vertex")]
VertexOutput vsMain(VertexInput input)
{
    float4 pos = float4(input.position, 1.0f);

    VertexOutput output;
    output.position = mul(cameraInfo.proj, mul(cameraInfo.view, mul(modelInfo.model, pos)));
    output.worldPosition = mul(modelInfo.model, pos);
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}

//
// Fragment
//

struct PointLight
{
    float3 position;
    float intensity;
    float3 color;
    bool cast_shadows;
};

struct DirectionalLight
{
    float3 position;
    float intensity;
    float3 color;
    bool cast_shadows;
    float3 direction;
};

[[vk::binding(0, 1)]]
StructuredBuffer<PointLight> pointLights : register(t0, space1);
[[vk::binding(1, 1)]]
StructuredBuffer<DirectionalLight> directionalLights : register(t1, space1);

[shader("fragment")]
float4 fsMain(VertexOutput input) 
{
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(cameraInfo.pos - input.worldPosition.xyz);
    float3 result = float3(0, 0, 0);                             

    for (uint i = 0; i < directionalLights.getCount(); i++) {
        DirectionalLight light = directionalLights[i];

        float3 lightDir = normalize(-light.direction);
        float diff = max(dot(normal, lightDir), 0.0); 

        float3 diffuse = light.intensity * light.color * diff;
        result += diffuse;
    }

    for (uint j = 0; j < pointLights.getCount(); j++) {
        PointLight light = pointLights[j];

        float3 lightDir = normalize(light.position - input.worldPosition.xyz);
        float distance = length(light.position - input.worldPosition.xyz);
        float attenuation = 1.0 / (distance * distance); 
        float diff = max(dot(normal, lightDir), 0.0);

        float3 diffuse = light.intensity * light.color * diff * attenuation;
        result += diffuse;
    }

    return float4(result, 1.0f);

}
