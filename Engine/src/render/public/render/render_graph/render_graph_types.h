#pragma once

#include <functional>
#include <memory>

#include "base/containers/inplace_vector.h"
#include "base/types/uuid.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/image_resource.h"

#define CREATE_RG_UUID_TYPE_BASE(name)                           \
    namespace Mizu                                               \
    {                                                            \
    struct name                                                  \
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

#define CREATE_RG_UUID_TYPE_INHERIT(name, inherit)             \
    namespace Mizu                                             \
    {                                                          \
    struct name : public inherit                               \
    {                                                          \
      public:                                                  \
        name() : inherit() {}                                  \
        name(UUID uuid) : inherit(uuid) {}                     \
        name(inherit parent) : inherit(parent) {}              \
                                                               \
        static name invalid()                                  \
        {                                                      \
            return UUID::invalid();                            \
        }                                                      \
    };                                                         \
    }                                                          \
    template <>                                                \
    struct std::hash<Mizu::name>                               \
    {                                                          \
        Mizu::UUID::Type operator()(const Mizu::name& k) const \
        {                                                      \
            return static_cast<Mizu::UUID::Type>(k);           \
        }                                                      \
    }

CREATE_RG_UUID_TYPE_BASE(RGResourceRef);

CREATE_RG_UUID_TYPE_INHERIT(RGImageRef, RGResourceRef);
CREATE_RG_UUID_TYPE_INHERIT(RGBufferRef, RGResourceRef);

CREATE_RG_UUID_TYPE_BASE(RGResourceViewRef);

CREATE_RG_UUID_TYPE_INHERIT(RGTextureViewRef, RGResourceViewRef);
CREATE_RG_UUID_TYPE_INHERIT(RGTextureSrvRef, RGTextureViewRef);
CREATE_RG_UUID_TYPE_INHERIT(RGTextureUavRef, RGTextureViewRef);
CREATE_RG_UUID_TYPE_INHERIT(RGTextureRtvRef, RGTextureViewRef);

CREATE_RG_UUID_TYPE_INHERIT(RGBufferViewRef, RGResourceViewRef);
CREATE_RG_UUID_TYPE_INHERIT(RGBufferSrvRef, RGBufferViewRef);
CREATE_RG_UUID_TYPE_INHERIT(RGBufferUavRef, RGBufferViewRef);
CREATE_RG_UUID_TYPE_INHERIT(RGBufferCbvRef, RGBufferViewRef);

CREATE_RG_UUID_TYPE_BASE(RGAccelerationStructureRef);

CREATE_RG_UUID_TYPE_BASE(RGResourceGroupRef);

#undef CREATE_RG_UUID_TYPE_INHERIT
#undef CREATE_RG_UUID_TYPE_BASE

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class BufferResource;
class CommandBuffer;
class ConstantBufferView;
class ImageResource;
class ResourceGroup;
class RGPassResources;
class ShaderResourceView;
class UnorderedAccessView;

using RGFunction = std::function<void(CommandBuffer&, const RGPassResources&)>;

using RGImageMap = std::unordered_map<RGImageRef, std::shared_ptr<ImageResource>>;
using RGBufferMap = std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>>;

using RGAccelerationStructureMap =
    std::unordered_map<RGAccelerationStructureRef, std::shared_ptr<AccelerationStructure>>;
using RGResourceGroupMap = std::unordered_map<RGResourceGroupRef, std::shared_ptr<ResourceGroup>>;

enum class RGPassHint
{
    Immediate,
    Raster,
    Compute,
    RayTracing,
};

struct RGFramebufferAttachments
{
    uint32_t width = 0, height = 0;

    inplace_vector<RGTextureRtvRef, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments{};
    RGTextureRtvRef depth_stencil_attachment = RGTextureRtvRef::invalid();
};

struct RGExternalTextureParams
{
    ImageResourceState input_state = ImageResourceState::ShaderReadOnly;
    ImageResourceState output_state = ImageResourceState::ShaderReadOnly;
};

struct RGExternalBufferParams
{
    BufferResourceState input_state = BufferResourceState::ShaderReadOnly;
    BufferResourceState output_state = BufferResourceState::ShaderReadOnly;
};

} // namespace Mizu
