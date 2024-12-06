#pragma once

#include <functional>
#include <memory>

#include "core/uuid.h"
#include "renderer/abstraction/compute_pipeline.h"
#include "renderer/abstraction/graphics_pipeline.h"

#define CREATE_RG_UUID_TYPE_INHERIT(name, inherit)                  \
    namespace Mizu {                                                \
    struct name : public inherit {                                  \
        name() : inherit() {}                                       \
        name(UUID uuid) : inherit(static_cast<UUID::Type>(uuid)) {} \
        explicit name(UUID::Type value) : inherit(value) {}         \
    };                                                              \
    }                                                               \
    template <>                                                     \
    struct std::hash<Mizu::name> {                                  \
        Mizu::UUID::Type operator()(const Mizu::name& k) const {    \
            return static_cast<Mizu::UUID::Type>(k);                \
        }                                                           \
    }

#define CREATE_RG_UUID_TYPE_BASE(name) CREATE_RG_UUID_TYPE_INHERIT(name, UUID)

namespace Mizu {

// Forward declarations
class RenderCommandBuffer;
class ComputeCommandBuffer;
class IMaterial;

// TODO: Change to "const RenderCommandBuffer&"
using RGFunction = std::function<void(RenderCommandBuffer&)>;

// using ApplyMaterialFunc = std::function<void(std::shared_ptr<RenderCommandBuffer>, const IMaterial&)>;
// using RGMaterialFunction = std::function<void(std::shared_ptr<RenderCommandBuffer>, ApplyMaterialFunc)>;

struct RGGraphicsPipelineDescription {
    RasterizationState rasterization{};
    DepthStencilState depth_stencil{};
    ColorBlendState color_blend{};
};

} // namespace Mizu

CREATE_RG_UUID_TYPE_BASE(RGImageRef);

CREATE_RG_UUID_TYPE_INHERIT(RGTextureRef, RGImageRef);
CREATE_RG_UUID_TYPE_INHERIT(RGCubemapRef, RGImageRef);

CREATE_RG_UUID_TYPE_BASE(RGBufferRef);

CREATE_RG_UUID_TYPE_BASE(RGFramebufferRef);
