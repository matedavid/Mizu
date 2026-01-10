#pragma once

#include <memory>
#include <span>
#include <variant>

#include "render_core/definitions/shader_types.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class SamplerState;

enum class DescriptorSetAllocationType
{
    Transient,
    Persistent,
    Bindless,
};

#define DESCRIPTOR_ITEMS_LIST                                                        \
    X(TextureSrv, ShaderResourceType::TextureSrv, ResourceView)                      \
    X(TextureUav, ShaderResourceType::TextureUav, ResourceView)                      \
    X(ConstantBuffer, ShaderResourceType::ConstantBuffer, ResourceView)              \
    X(StructuredBufferSrv, ShaderResourceType::StructuredBufferSrv, ResourceView)    \
    X(StructuredBufferUav, ShaderResourceType::StructuredBufferUav, ResourceView)    \
    X(ByteAddressBufferSrv, ShaderResourceType::ByteAddressBufferSrv, ResourceView)  \
    X(ByteAddressBufferUav, ShaderResourceType::ByteAddressBufferUav, ResourceView)  \
    X(SamplerState, ShaderResourceType::SamplerState, std::shared_ptr<SamplerState>) \
    X(RtxAccelerationStructure, ShaderResourceType::AccelerationStructure, std::shared_ptr<AccelerationStructure>)

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

struct DescriptorItem
{
#define X(_name, _type, _value_type)                                                              \
    static DescriptorItem _name(uint32_t binding, uint32_t count, ShaderType stage)               \
    {                                                                                             \
        return DescriptorItem{.binding = binding, .count = count, .stage = stage, .type = _type}; \
    }

    DESCRIPTOR_ITEMS_LIST

#undef X

    uint32_t binding;
    uint32_t count;
    ShaderType stage;
    ShaderResourceType type;
};

#undef DESCRIPTOR_ITEMS_LIST

class DescriptorSet
{
  public:
    virtual ~DescriptorSet() = default;

    virtual void update(std::span<WriteDescriptor> writes, uint32_t array_offset = 0) = 0;
};

} // namespace Mizu