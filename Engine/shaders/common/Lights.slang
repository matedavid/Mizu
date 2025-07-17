struct PointLight
{
    float3 position;
    float intensity;
    float3 color;
    bool cast_shadows;
    float radius;
};

struct DirectionalLight
{
    float3 position;
    float intensity;
    float3 color;
    bool cast_shadows;
    float3 direction;
};
