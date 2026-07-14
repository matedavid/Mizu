#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "render/render_graph/render_graph_types.h"
#include "render/systems/frame_linear_allocator.h"

namespace Mizu
{

class FrameLinearAllocator;
class MaterialResidencySystem;
class TextureResidencySystem;
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
    FrameAllocation camera_allocation;
    RenderGraphResource view_output_texture;
};

struct GpuCameraInfo
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::mat4 inverseView;
    glm::mat4 inverseProj;
    glm::mat4 inverseViewProj;
    glm::vec3 pos;

    float _pad0{};

    float znear;
    float zfar;

    glm::vec2 _pad1{};
};

struct LightsData
{
    FrameAllocation point_lights_allocation;
    FrameAllocation directional_lights_allocation;
};

} // namespace Mizu
