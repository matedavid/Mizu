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

using WriteDescriptorValueT =
    std::variant<ResourceView, std::shared_ptr<SamplerState>, std::shared_ptr<AccelerationStructure>>;
struct WriteDescriptor
{
    uint32_t binding;
    ShaderResourceType type;
    WriteDescriptorValueT value;
};

struct DescriptorItem
{
#define DESCRIPTOR_ITEMS_LIST                                       \
    X(TextureSrv, ShaderResourceType::TextureSrv)                   \
    X(TextureUav, ShaderResourceType::TextureUav)                   \
    X(ConstantBuffer, ShaderResourceType::ConstantBuffer)           \
    X(StructuredBufferSrv, ShaderResourceType::StructuredBufferSrv) \
    X(StructuredBufferUav, ShaderResourceType::StructuredBufferUav) \
    X(SamplerState, ShaderResourceType::SamplerState)               \
    X(RtxAccelerationStructure, ShaderResourceType::AccelerationStructure)

#define X(_name, _type)                                                                           \
    static DescriptorItem _name(uint32_t binding, uint32_t count, ShaderType stage)               \
    {                                                                                             \
        return DescriptorItem{.binding = binding, .count = count, .stage = stage, .type = _type}; \
    }

    DESCRIPTOR_ITEMS_LIST

#undef X
#undef DESCRIPTOR_ITEMS_LIST

    uint32_t binding;
    uint32_t count;
    ShaderType stage;
    ShaderResourceType type;
};

class DescriptorSet
{
  public:
    virtual ~DescriptorSet() = default;

    virtual void update(std::span<WriteDescriptor> writes, uint32_t array_offset = 0) = 0;
};

} // namespace Mizu