#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "base/debug/assert.h"
#include "render_core/rhi/descriptors.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/rhi/render_pass.h"
#include "shader/shader_declaration.h"

#include "render/resources/gpu_resource_types.h"

namespace Mizu
{

struct DrawElement
{
    GpuMeshDrawPayload mesh_draw{};

    ShaderInstance vertex_instance{};
    ShaderInstance fragment_instance{};

    uint32_t instance_count = 0;
    uint32_t material_buffer_offset = std::numeric_limits<uint32_t>::max();
    uint32_t transform_buffer_offset = std::numeric_limits<uint32_t>::max();
    uint32_t view_indices_offset = std::numeric_limits<uint32_t>::max();

    size_t sort_key = 0;
    size_t pipeline_hash = 0;
};

struct DrawListRasterBindings
{
    std::array<std::shared_ptr<DescriptorSet>, MAX_DESCRIPTOR_SET_COUNT> descriptor_sets{};

    DrawListRasterBindings& add(uint32_t set, std::shared_ptr<DescriptorSet> descriptor_set)
    {
        MIZU_ASSERT(
            set < MAX_DESCRIPTOR_SET_COUNT,
            "Set is higher than the max descriptor set count ({} >= {})",
            set,
            MAX_DESCRIPTOR_SET_COUNT);

        if (descriptor_sets[set] != nullptr)
        {
            MIZU_ASSERT(false, "Already added descriptor set at set {}", set);
            return *this;
        }

        descriptor_sets[set] = std::move(descriptor_set);
        return *this;
    }
};

struct DrawListRasterPassInfo
{
    RasterizationState rasterization_state{};
    DepthStencilState depth_stencil_state{};
    ColorBlendState color_blend_state{};
    FramebufferInfo framebuffer_info{};
    DrawListRasterBindings bindings{};
};

} // namespace Mizu