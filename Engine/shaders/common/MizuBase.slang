#define MIZU_SRV_BINDING_OFFSET 0
#define MIZU_UAV_BINDING_OFFSET 16
#define MIZU_CBV_BINDING_OFFSET 32
#define MIZU_SAMPLER_BINDING_OFFSET 64

#define MIZU_VK_BINDING_SRV(_binding, _set) [[vk::binding(_binding + MIZU_SRV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_UAV(_binding, _set) [[vk::binding(_binding + MIZU_UAV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_CBV(_binding, _set) [[vk::binding(_binding + MIZU_CBV_BINDING_OFFSET, _set)]]
#define MIZU_VK_BINDING_SAMPLER(_binding, _set) [[vk::binding(_binding + MIZU_SAMPLER_BINDING_OFFSET, _set)]]

float3 viewSpacePositionFromDepth(float2 uv, float depth, float4x4 inverseProjection)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = uv.y * 2.0f - 1.0f;

    float4 projectedPos = float4(x, y, depth, 1.0f);
    float4 viewSpacePosition = mul(inverseProjection, projectedPos);

    return viewSpacePosition.xyz / viewSpacePosition.w;
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

