struct PointLight
{
    float3 position;
    float intensity;
    float3 color;
    float castShadows;
    float radius;

    // Using float array insted of float3 because std430 layout creates new alignment requirement for float3
    float _padding[3];
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
