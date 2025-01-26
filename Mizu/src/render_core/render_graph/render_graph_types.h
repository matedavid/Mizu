#pragma once

#include <functional>
#include <memory>

#include "core/uuid.h"
#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"

#define CREATE_RG_UUID_TYPE_INHERIT(name, inherit)                  \
    namespace Mizu                                                  \
    {                                                               \
    struct name : public inherit                                    \
    {                                                               \
        name() : inherit() {}                                       \
        name(UUID uuid) : inherit(static_cast<UUID::Type>(uuid)) {} \
        explicit name(UUID::Type value) : inherit(value) {}         \
    };                                                              \
    }                                                               \
    template <>                                                     \
    struct std::hash<Mizu::name>                                    \
    {                                                               \
        Mizu::UUID::Type operator()(const Mizu::name& k) const      \
        {                                                           \
            return static_cast<Mizu::UUID::Type>(k);                \
        }                                                           \
    }

#define CREATE_RG_UUID_TYPE_BASE(name) CREATE_RG_UUID_TYPE_INHERIT(name, UUID)

namespace Mizu
{

// Forward declarations
class RenderCommandBuffer;

using RGFunction = std::function<void(RenderCommandBuffer&)>;

struct RGGraphicsPipelineDescription
{
    RasterizationState rasterization{};
    DepthStencilState depth_stencil{};
    ColorBlendState color_blend{};
};

} // namespace Mizu

CREATE_RG_UUID_TYPE_BASE(RGImageRef);

CREATE_RG_UUID_TYPE_INHERIT(RGTextureRef, RGImageRef);
CREATE_RG_UUID_TYPE_INHERIT(RGCubemapRef, RGImageRef);

CREATE_RG_UUID_TYPE_BASE(RGBufferRef);

CREATE_RG_UUID_TYPE_INHERIT(RGUniformBufferRef, RGBufferRef);
CREATE_RG_UUID_TYPE_INHERIT(RGStorageBufferRef, RGBufferRef);

CREATE_RG_UUID_TYPE_BASE(RGFramebufferRef);
