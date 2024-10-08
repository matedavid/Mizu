#pragma once

#include <functional>
#include <memory>

#include "core/uuid.h"
#include "renderer/abstraction/graphics_pipeline.h"

#define CREATE_RG_UUID_TYPE(name)                                \
    namespace Mizu {                                             \
    struct name : public UUID {                                  \
        name() : UUID() {}                                       \
        name(UUID uuid) : UUID(static_cast<UUID::Type>(uuid)) {} \
        explicit name(UUID::Type value) : UUID(value) {}         \
    };                                                           \
    }                                                            \
    template <>                                                  \
    struct std::hash<Mizu::name> {                               \
        Mizu::UUID::Type operator()(const Mizu::name& k) const { \
            return static_cast<Mizu::UUID::Type>(k);             \
        }                                                        \
    }

namespace Mizu {

// Forward declarations
class RenderCommandBuffer;
class ComputeCommandBuffer;

using RGFunction = std::function<void(std::shared_ptr<RenderCommandBuffer> command_buffer)>;

struct RGGraphicsPipelineDescription {
    RasterizationState rasterization{};
    DepthStencilState depth_stencil{};
    ColorBlendState color_blend{};
};

} // namespace Mizu

CREATE_RG_UUID_TYPE(RGTextureRef);
CREATE_RG_UUID_TYPE(RGCubemapRef);
CREATE_RG_UUID_TYPE(RGUniformBufferRef);
CREATE_RG_UUID_TYPE(RGFramebufferRef);
