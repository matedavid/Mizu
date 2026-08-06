#include "base/constants.slang"

namespace PbrCommon
{

float3 fresnel_schlick(float cos_theta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(saturate(1.0 - cos_theta), 5.0);
}

float3 fresnel_schlick_roughness(float cos_theta, float3 f0, float roughness)
{
    return f0 + (max(float3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_ggx(float3 n, float3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float n_dot_h = max(dot(n, h), 0.0);
    float n_dot_h2 = n_dot_h * n_dot_h;

    float num = a2;
    float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometry_schlick_ggx(float n_dot_v, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = n_dot_v * (1.0 - k) + k;
    return n_dot_v / denom;
}

float geometry_smith(float3 n, float3 v, float3 l, float roughness)
{
    float n_dot_l = max(dot(n, l), 0.0);
    float n_dot_v = max(dot(n, v), 0.0);

    float ggx1 = geometry_schlick_ggx(n_dot_l, roughness);
    float ggx2 = geometry_schlick_ggx(n_dot_v, roughness);

    return ggx1 * ggx2;
}

struct PbrMaterial
{
    float3 position;
    float3 n;

    float3 albedo;
    float metallic;
    float roughness;
    float ao;
};

float3 pbr_calculation(PbrMaterial material, float3 v, float3 l, float3 f0)
{
    float3 h = normalize(v + l);

    // cook-torrance brdf
    float ndf = PbrCommon::distribution_ggx(material.n, h, material.roughness);
    float g = PbrCommon::geometry_smith(material.n, v, l, material.roughness);
    float3 f = PbrCommon::fresnel_schlick(saturate(dot(h, v)), f0);

    float3 numerator = ndf * g * f;
    float denominator = 4.0 * max(dot(material.n, v), 0.0) * max(dot(material.n, l), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float3 k_s = f;
    // Because energy conservation, the diffuse and specular light can't be above 1.0
    float3 k_d = float3(1.0) - k_s;
    // Enforce that metallic sufraces dont refract light
    k_d *= 1.0 - material.metallic;

    float n_dot_l = max(dot(material.n, l), 0.0);
    return (k_d * material.albedo / PI + specular) * n_dot_l;
}

}
