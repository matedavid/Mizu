#include "base/descriptor_bindings.slang"

MIZU_VK_BINDING_SRV(0, 0)
Texture2D<float4> g_pending_texture : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
Texture2D<float4> g_reference_texture : register(t1, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<float> g_result_buffer : register(u0, space0);

float mean_squared_error(float4 pending, float4 reference)
{
    float4 diff = reference - pending;
    float4 diff_sq = diff * diff;
    return dot(diff_sq, float4(1.0, 1.0, 1.0, 1.0));
}

[numthreads(8, 8, 1)]
void cs_compare_images(uint3 id: SV_DispatchThreadID)
{
    uint width, height;
    g_pending_texture.GetDimensions(width, height);

    if (id.x >= width || id.y >= height)
        return;

    float4 pending_color = g_pending_texture.Load(int3(id.xy, 0));
    float4 reference_color = g_reference_texture.Load(int3(id.xy, 0));

    float mse = mean_squared_error(pending_color, reference_color);
    g_result_buffer[id.y * width + id.x] = mse;
}
