#pragma once

#include <functional>
#include <memory>

#include "core/uuid.h"
#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"

#define CREATE_RG_UUID_TYPE(name, optional_include)              \
    namespace Mizu                                               \
    {                                                            \
    struct name optional_include                                 \
    {                                                            \
        name() : m_uuid(UUID()) {}                               \
        name(UUID uuid) : m_uuid(uuid) {}                        \
        explicit name(UUID::Type value) : m_uuid(UUID(value)) {} \
        static name invalid()                                    \
        {                                                        \
            return UUID::invalid();                              \
        }                                                        \
        operator UUID::Type() const                              \
        {                                                        \
            return static_cast<UUID::Type>(m_uuid);              \
        }                                                        \
        bool operator==(const name& other) const                 \
        {                                                        \
            return m_uuid == other.m_uuid;                       \
        }                                                        \
                                                                 \
      private:                                                   \
        UUID m_uuid;                                             \
    };                                                           \
    }                                                            \
    template <>                                                  \
    struct std::hash<Mizu::name>                                 \
    {                                                            \
        Mizu::UUID::Type operator()(const Mizu::name& k) const   \
        {                                                        \
            return static_cast<Mizu::UUID::Type>(k);             \
        }                                                        \
    }

#define CREATE_RG_UUID_TYPE_BASE(name) CREATE_RG_UUID_TYPE(name, )
#define CREATE_RG_UUID_TYPE_INHERIT(name, inherit) CREATE_RG_UUID_TYPE(name, : public inherit)

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
