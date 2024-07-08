#pragma once

#include <functional>
#include <memory>

#include "core/uuid.h"
#include "renderer/abstraction/graphics_pipeline.h"

namespace Mizu {

// Forward declarations
class RenderCommandBuffer;
class Shader;

using RGFunction = std::function<void(std::shared_ptr<RenderCommandBuffer> command_buffer)>;
using RGTextureRef = UUID;
using RGFramebufferRef = UUID;

struct RGGraphicsPipelineDescription {
    std::shared_ptr<Shader> shader;

    RasterizationState rasterization{};
    DepthStencilState depth_stencil{};
    ColorBlendState color_blend{};
};

} // namespace Mizu