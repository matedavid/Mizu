#pragma once

#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <variant>

#include "base/types/uuid.h"

#include "render_core/definitions/shader_types.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class SamplerState;

// clang-format off
#define MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(_name)                       \
    struct _name                                                      \
    {                                                                 \
      public:                                                         \
        static std::span<const DescriptorItem> get_layout()           \
        {                                                             \
            static constexpr Mizu::DescriptorItem layout[] = {

#define MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(_binding, _count, _stage) \
    Mizu::DescriptorItem::TextureSrv(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_UAV(_binding, _count, _stage) \
    Mizu::DescriptorItem::TextureUav(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(_binding, _count, _stage) \
    Mizu::DescriptorItem::ConstantBuffer(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(_binding, _count, _stage) \
    Mizu::DescriptorItem::StructuredBufferSrv(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(_binding, _count, _stage) \
    Mizu::DescriptorItem::StructuredBufferUav(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_BYTE_ADDRESS_BUFFER_SRV(_binding, _count, _stage) \
    Mizu::DescriptorItem::ByteAddressBufferSrv(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_BYTE_ADDRESS_BUFFER_UAV(_binding, _count, _stage) \
    Mizu::DescriptorItem::ByteAddressBufferUav(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(_binding, _count, _stage) \
    Mizu::DescriptorItem::SamplerState(_binding, _count, _stage),

#define MIZU_DESCRIPTOR_SET_LAYOUT_ACCELERATION_STRUCTURE(_binding, _count, _stage) \
    Mizu::DescriptorItem::AccelerationStructure(_binding, _count, _stage),

#define MIZU_END_DESCRIPTOR_SET_LAYOUT() \
    }; return layout; } };
// clang-format on

#define DESCRIPTOR_ITEMS_LIST                                                        \
    X(TextureSrv, ShaderResourceType::TextureSrv, ResourceView)                      \
    X(TextureUav, ShaderResourceType::TextureUav, ResourceView)                      \
    X(ConstantBuffer, ShaderResourceType::ConstantBuffer, ResourceView)              \
    X(StructuredBufferSrv, ShaderResourceType::StructuredBufferSrv, ResourceView)    \
    X(StructuredBufferUav, ShaderResourceType::StructuredBufferUav, ResourceView)    \
    X(ByteAddressBufferSrv, ShaderResourceType::ByteAddressBufferSrv, ResourceView)  \
    X(ByteAddressBufferUav, ShaderResourceType::ByteAddressBufferUav, ResourceView)  \
    X(SamplerState, ShaderResourceType::SamplerState, std::shared_ptr<SamplerState>) \
    X(AccelerationStructure, ShaderResourceType::AccelerationStructure, ResourceView)

struct DescriptorItem
{
#define X(_name, _type, _value_type)                                                              \
    static constexpr DescriptorItem _name(uint32_t binding, uint32_t count, ShaderType stage)     \
    {                                                                                             \
        return DescriptorItem{.binding = binding, .count = count, .stage = stage, .type = _type}; \
    }

    DESCRIPTOR_ITEMS_LIST

#undef X

    size_t hash() const { return hash_compute(binding, count, stage, type); } // namespace Mizu

    uint32_t binding;
    uint32_t count;
    ShaderType stage;
    ShaderResourceType type;
};

using WriteDescriptorValueT =
    std::variant<ResourceView, std::shared_ptr<SamplerState>, std::shared_ptr<AccelerationStructure>>;
struct WriteDescriptor
{
#define X(_name, _type, _value_type)                                               \
    static WriteDescriptor _name(uint32_t binding, _value_type value)              \
    {                                                                              \
        return WriteDescriptor{.binding = binding, .type = _type, .value = value}; \
    }

    DESCRIPTOR_ITEMS_LIST

#undef X

    uint32_t binding;
    ShaderResourceType type;
    WriteDescriptorValueT value;
};

#undef DESCRIPTOR_ITEMS_LIST

struct PushConstantItem
{
    uint32_t size;
    ShaderType stage;

    size_t hash() const { return hash_compute(size, stage); }
};

enum class DescriptorSetAllocationType
{
    Transient,
    Persistent,
    Bindless,
};

constexpr uint32_t BINDLESS_DESCRIPTOR_COUNT = std::numeric_limits<uint32_t>::max();

using DescriptorSetLayoutHandle = size_t;
using PipelineLayoutHandle = size_t;

struct DescriptorSetLayoutDescription
{
    std::span<const DescriptorItem> layout;

    // Only in Vulkan, if we want to apply the binding type based offsets.
    bool vulkan_apply_binding_offsets = true;
};

struct PipelineLayoutDescription
{
    std::span<DescriptorSetLayoutHandle> set_layouts;
    std::optional<PushConstantItem> push_constant;
};

class DescriptorSet
{
  public:
    virtual ~DescriptorSet() = default;

    virtual void update(std::span<const WriteDescriptor> writes, uint32_t array_offset = 0) = 0;
};

} // namespace Mizu
