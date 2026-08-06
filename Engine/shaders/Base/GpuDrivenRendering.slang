#ifndef BASE_GPU_DRIVEN_RENDERING
#define BASE_GPU_DRIVEN_RENDERING

struct DrawIndexedIndirectCommand
{
    uint32_t draw_index;
    uint32_t index_count;
    uint32_t instance_count;
    uint32_t first_index;
    int32_t vertex_offset;
    uint32_t first_instance;
};

struct GpuDrawData
{
    uint32_t transformSlot;
    uint32_t materialOffset;
};

#if defined(MIZU_TARGET_DX12)

struct MizuDrawIndexRootConstant
{
    uint32_t value;
};

// register/space must match DRAW_INDIRECT_INDEX_ROOT_CONSTANT_REGISTER/SPACE in dx12_root_signature.cpp
#define DRAW_INDIRECT_INDEX_ROOT_CONSTANT_REGISTER b0
#define DRAW_INDIRECT_INDEX_ROOT_CONSTANT_SPACE space6

[[vk::binding(0, 6)]] // Not used, just to make the compiler happy
ConstantBuffer<MizuDrawIndexRootConstant> g_mizuDrawIndex
    : register(DRAW_INDIRECT_INDEX_ROOT_CONSTANT_REGISTER, DRAW_INDIRECT_INDEX_ROOT_CONSTANT_SPACE);

#define MIZU_DRAW_INDEX_PARAM
#define MIZU_GET_SV_DRAW_INDEX() (g_mizuDrawIndex.value)

#elif defined(MIZU_TARGET_SPIRV)

#define MIZU_DRAW_INDEX_PARAM , uint32_t _mizuSvDrawIndex : SV_DrawIndex
#define MIZU_GET_SV_DRAW_INDEX() (_mizuSvDrawIndex)

#else

#error "Unsupported target"

#endif

#endif 