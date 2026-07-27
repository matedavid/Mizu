#ifndef BASE_UTILS
#define BASE_UTILS

#include "Base/Constants.slang"

float3 viewSpacePositionFromDepth(float2 uv, float depth, float4x4 inverseProj)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = 1.0f - uv.y * 2.0f;

    float4 projectedPos = float4(x, y, depth, 1.0f);
    float4 viewSpacePosition = mul(inverseProj, projectedPos);

    return viewSpacePosition.xyz / viewSpacePosition.w;
}

float3 worldSpacePositionFromDepth(float2 uv, float depth, float4x4 inverseViewProj)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = 1.0f - uv.y * 2.0f;

    float4 projectedPos = float4(x, y, depth, 1.0f);
    float4 worldSpacePos = mul(inverseViewProj, projectedPos);

    return worldSpacePos.xyz / worldSpacePos.w;
}

float3x3 inverse(float3x3 m)
{
    float det = m._11 * (m._22 * m._33 - m._23 * m._32) - m._12 * (m._21 * m._33 - m._23 * m._31)
                + m._13 * (m._21 * m._32 - m._22 * m._31);

    if (det == 0.0)
    {
        return float3x3(0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    float invDet = 1.0 / det;

    float3x3 adjugate;
    adjugate._11 = (m._22 * m._33 - m._23 * m._32);
    adjugate._12 = -(m._12 * m._33 - m._13 * m._32);
    adjugate._13 = (m._12 * m._23 - m._13 * m._22);

    adjugate._21 = -(m._21 * m._33 - m._23 * m._31);
    adjugate._22 = (m._11 * m._33 - m._13 * m._31);
    adjugate._23 = -(m._11 * m._23 - m._13 * m._21);

    adjugate._31 = (m._21 * m._32 - m._22 * m._31);
    adjugate._32 = -(m._11 * m._32 - m._12 * m._31);
    adjugate._33 = (m._11 * m._22 - m._12 * m._21);

    return adjugate * invDet;
}

float2 encodeNormalSpherical(float3 n)
{
    n = normalize(n);

    float azimuth = atan2(n.y, n.x);                 // [-pi, pi]
    float elevation = acos(clamp(n.z, -1.0f, 1.0f)); // [0, pi]

    return float2(azimuth / (2.0f * PI) + 0.5f, elevation / PI);
}

float3 decodeNormalSpherical(float2 e)
{
    float azimuth = (e.x - 0.5f) * (2.0f * PI);
    float elevation = e.y * PI;

    float sinE = sin(elevation);
    return float3(cos(azimuth) * sinE, sin(azimuth) * sinE, cos(elevation));
}

#endif 