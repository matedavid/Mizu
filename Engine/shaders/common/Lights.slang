struct PointLight
{
    float3 position;
    float intensity;
    float3 color;
    bool castShadows;
    float radius;
};

struct DirectionalLight
{
    float3 position;
    float intensity;
    float3 color;
    bool castShadows;
    float3 direction;
};
