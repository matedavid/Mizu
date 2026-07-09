#include "Base/DescriptorBindings.slang"

MIZU_VK_BINDING_SRV(0, 0)
Texture2D<float4> pendingTexture : register(t0, space0);

MIZU_VK_BINDING_SRV(1, 0)
Texture2D<float4> referenceTexture : register(t1, space0);

MIZU_VK_BINDING_UAV(0, 0)
RWStructuredBuffer<float> resultBuffer : register(u0, space0);

float meanSquaredError(float4 pending, float4 reference)
{
    float4 diff = reference - pending;
    float4 diffSq = diff * diff;
    return dot(diffSq, float4(1.0, 1.0, 1.0, 1.0));
}

[numthreads(8, 8, 1)]
void compareImagesCs(uint3 id: SV_DispatchThreadID)
{
    uint width, height;
    pendingTexture.GetDimensions(width, height);

    if (id.x >= width || id.y >= height)
        return;

    float4 pendingColor = pendingTexture.Load(int3(id.xy, 0));
    float4 referenceColor = referenceTexture.Load(int3(id.xy, 0));

    float mse = meanSquaredError(pendingColor, referenceColor);
    resultBuffer[id.y * width + id.x] = mse;
}
