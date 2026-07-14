float3 viewSpacePositionFromDepth(float2 uv, float depth, float4x4 inverseProj)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = uv.y * 2.0f - 1.0f;

    float4 projectedPos = float4(x, y, depth, 1.0f);
    float4 viewSpacePosition = mul(inverseProj, projectedPos);

    return viewSpacePosition.xyz / viewSpacePosition.w;
}

float3 worldSpacePositionFromDepth(float2 uv, float depth, float4x4 inverseViewProj)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = uv.y * 2.0f - 1.0f;

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

// Same as sign() but, when the value is zero, return 1 instead of 0
// e.g. 0 is positive
T signNotZero<T : IFloat>(T v)
{
    return select(v >= T(0.0f), T(1.0f), T(-1.0f));
}

half2 encodeNormalOctahedralEncoding(float3 normal)
{
    normal /= (abs(normal.x) + abs(normal.y) + abs(normal.z));

    float2 encoded = (normal.z >= 0.0f) ? normal.xy : (1.0f - abs(normal.yx)) * signNotZero(normal.xy);
    return half2(encoded);
}

float3 decodeNormalOctahedralEncoding(half2 normal)
{
    float3 decoded = float3(normal.x, normal.y, 1.0f - abs(normal.x) - abs(normal.y));

    float t = saturate(-decoded.z);
    decoded.xy += select(decoded.xy >= 0.0f, -t, t);

    return normalize(decoded);
}
