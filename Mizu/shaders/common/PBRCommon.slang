static float PI = 3.14159265359;

namespace PBRCommon
{

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float distributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}

struct PBRMaterial
{
    float3 position;
    float3 N;

    float3 albedo;
    float metallic;
    float roughness;
    float ao;
}

float3 PBRCalculation(PBRMaterial material, float3 V, float3 L, float3 F0)
{
    float3 H = normalize(V + L);

    // cook-torrance brdf
    float NDF = PBRCommon::distributionGGX(material.N, H, material.roughness);
    float G = PBRCommon::geometrySmith(material.N, V, L, material.roughness);
    float3 F = PBRCommon::fresnelSchlick(saturate(dot(H, V)), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(material.N, V), 0.0) * max(dot(material.N, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float3 kS = F;
    // Because energy conservation, the diffuse and specular light can't be above 1.0
    float3 kD = float3(1.0) - kS;
    // Enforce that metallic sufraces dont refract light
    kD *= 1.0 - material.metallic;

    float NdotL = max(dot(material.N, L), 0.0);
    return (kD * material.albedo / PI + specular) * NdotL;
}

}
