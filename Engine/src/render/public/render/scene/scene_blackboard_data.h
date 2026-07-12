#pragma once

#include <cstdint>

namespace Mizu
{

class FrameLinearAllocator;
class MaterialResidencySystem;
class TextureResidencySystem;
struct RenderGraphResource;
struct RenderViewRegistryEntry;

struct FrameData
{
    uint64_t frame_num;
    uint32_t frame_in_flight_idx;
    double last_frame_seconds;
};

struct RenderSystemsData
{
    FrameLinearAllocator& frame_allocator;
    TextureResidencySystem& texture_residency_system;
    MaterialResidencySystem& material_residency_system;
};

struct RenderViewData
{
    const RenderViewRegistryEntry& data;

    uint32_t width, height;
    uint32_t offsetx, offsety;
    uint32_t layer;
    RenderGraphResource view_output_texture;
};

} // namespace Mizu
