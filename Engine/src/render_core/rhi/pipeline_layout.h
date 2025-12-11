#pragma once

#include <span>

#include "base/utils/hash.h"

#include "render_core/rhi/shader.h"
#include "render_core/shader/shader_types.h"

namespace Mizu
{

#define DESCRIPTOR_BINDING_INFO_LIST                                  \
    X(TextureSrv, ShaderResourceType::TextureSrv)                     \
    X(TextureUav, ShaderResourceType::TextureUav)                     \
    X(StructuredBufferSrv, ShaderResourceType::StructuredBufferSrv)   \
    X(StructuredBufferUav, ShaderResourceType::StructuredBufferUav)   \
    X(ByteAddressBufferSrv, ShaderResourceType::ByteAddressBufferSrv) \
    X(ByteAddressBufferUav, ShaderResourceType::ByteAddressBufferUav) \
    X(ConstantBuffer, ShaderResourceType::ConstantBuffer)             \
    X(SamplerState, ShaderResourceType::SamplerState)

struct DescriptorBindingInfo
{
    ShaderBindingInfo binding_info;
    ShaderResourceType type;
    uint32_t size;
    ShaderType stage;

#define X(_name, _type)                                                                                 \
    static DescriptorBindingInfo _name(ShaderBindingInfo binding_info, uint32_t size, ShaderType stage) \
    {                                                                                                   \
        return DescriptorBindingInfo{binding_info, _type, size, stage};                                 \
    }

    DESCRIPTOR_BINDING_INFO_LIST

#undef X

    static DescriptorBindingInfo PushConstant(uint32_t size, ShaderType stage)
    {
        MIZU_ASSERT(size % 4 == 0, "Push constant size must be a multiple of 4");

        // TODO: Think what to do with the ShaderBindingInfo binding position, as in d3d12 it matters compared to other
        // cbv descriptors. Currently setting to 1 for testing purposes.
        return DescriptorBindingInfo{ShaderBindingInfo{0, 1}, ShaderResourceType::PushConstant, size, stage};
    }

    size_t hash() const { return hash_compute(binding_info.set, binding_info.binding, type, size, stage); }

    // Don't like this default constructor, but it's useful in some cases...
    DescriptorBindingInfo() = default;

  private:
    DescriptorBindingInfo(ShaderBindingInfo binding_info, ShaderResourceType type_, uint32_t size_, ShaderType stage_)
        : binding_info(binding_info)
        , type(type_)
        , size(size_)
        , stage(stage_)
    {
    }
};

#undef DESCRIPTOR_BINDING_INFO_LIST

} // namespace Mizu