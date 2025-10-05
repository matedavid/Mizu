#pragma once

#include <functional>
#include <memory>

#include "base/types/uuid.h"

#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/image_resource.h"

#define CREATE_RG_UUID_TYPE(name, optional_include)

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

#define CREATE_RG_UUID_TYPE_INHERIT(name, inherit) \
    namespace Mizu                                 \
    {                                              \
    class name : public inherit                    \
    {                                              \
      public:                                      \
        name() : inherit() {}                      \
        name(UUID uuid) : inherit(uuid) {}         \
                                                   \
        static name invalid()                      \
        {                                          \
            return UUID::invalid();                \
        }                                          \
    };                                             \
    }

CREATE_RG_UUID_TYPE_BASE(RGImageRef);
CREATE_RG_UUID_TYPE_BASE(RGImageViewRef);

CREATE_RG_UUID_TYPE_BASE(RGBufferRef);

CREATE_RG_UUID_TYPE_INHERIT(RGUniformBufferRef, RGBufferRef);
CREATE_RG_UUID_TYPE_INHERIT(RGStorageBufferRef, RGBufferRef);

CREATE_RG_UUID_TYPE_BASE(RGAccelerationStructureRef);

CREATE_RG_UUID_TYPE_BASE(RGResourceGroupRef);

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class RGPassResources;
class ImageResourceView;
class BufferResource;
class ImageResource;
class AccelerationStructure;
class ResourceGroup;

using RGFunction = std::function<void(CommandBuffer&, const RGPassResources&)>;

using RGImageMap = std::unordered_map<RGImageRef, std::shared_ptr<ImageResource>>;
using RGImageViewMap = std::unordered_map<RGImageViewRef, std::shared_ptr<ImageResourceView>>;
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

    std::vector<RGImageViewRef> color_attachments{};
    RGImageViewRef depth_stencil_attachment = RGImageViewRef::invalid();
};

struct RGExternalTextureParams
{
    ImageResourceState input_state = ImageResourceState::ShaderReadOnly;
    ImageResourceState output_state = ImageResourceState::ShaderReadOnly;
};

} // namespace Mizu
