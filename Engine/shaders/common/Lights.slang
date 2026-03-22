struct PointLight
{
    float3 position;
    float intensity;
    float3 color;
    float castShadows;
    float radius;

    float3 _padding;
};

struct DirectionalLight
{
    float3 position;
    float intensity;
    float3 color;
    float castShadows;
    float3 direction;

    float _padding;
};
