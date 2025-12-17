#include "backend/dx12/dx12_shader.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/io/filesystem.h"

namespace Mizu::Dx12
{

Dx12Shader::Dx12Shader(ShaderDescription desc) : m_description(std::move(desc))
{
    m_source_code = Filesystem::read_file(m_description.path);

    m_shader_bytecode = {};
    m_shader_bytecode.pShaderBytecode = m_source_code.data();
    m_shader_bytecode.BytecodeLength = m_source_code.size();
}

Dx12Shader::~Dx12Shader()
{
    // Destructor here to destruct m_reflection correctly
}

D3D12_SHADER_VISIBILITY Dx12Shader::get_dx12_shader_stage_bits(ShaderType stage)
{
    switch (stage)
    {
    case ShaderType::Vertex:
        return D3D12_SHADER_VISIBILITY_VERTEX;
    case ShaderType::Fragment:
        return D3D12_SHADER_VISIBILITY_PIXEL;
    default:
        // There are no combined visibilities, nor compute, so the rest of types, or Vertex | Fragment, use ALL
        return D3D12_SHADER_VISIBILITY_ALL;
    }
}

D3D12_DESCRIPTOR_RANGE_TYPE Dx12Shader::get_dx12_descriptor_type(const ShaderResourceT& value)
{
    if (std::holds_alternative<ShaderResourceTexture>(value))
    {
        const ShaderResourceTexture& texture = std::get<ShaderResourceTexture>(value);

        switch (texture.access)
        {
        case ShaderResourceAccessType::ReadOnly:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case ShaderResourceAccessType::ReadWrite:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }
    }
    else if (std::holds_alternative<ShaderResourceTextureCube>(value))
    {
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }
    else if (std::holds_alternative<ShaderResourceStructuredBuffer>(value))
    {
        const ShaderResourceStructuredBuffer& structured_buffer = std::get<ShaderResourceStructuredBuffer>(value);

        switch (structured_buffer.access)
        {
        case ShaderResourceAccessType::ReadOnly:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case ShaderResourceAccessType::ReadWrite:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }
    }
    else if (std::holds_alternative<ShaderResourceByteAddressBuffer>(value))
    {
        const ShaderResourceByteAddressBuffer& byte_address_buffer = std::get<ShaderResourceByteAddressBuffer>(value);

        switch (byte_address_buffer.access)
        {
        case ShaderResourceAccessType::ReadOnly:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case ShaderResourceAccessType::ReadWrite:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }
    }
    else if (std::holds_alternative<ShaderResourceConstantBuffer>(value))
    {
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    }
    else if (std::holds_alternative<ShaderResourceSamplerState>(value))
    {
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    }

    MIZU_UNREACHABLE("ShaderResourceT should only have specified types in variant");

    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // Default to prevent compilation errors
}

D3D12_DESCRIPTOR_RANGE_TYPE Dx12Shader::get_dx12_descriptor_type(ShaderResourceType type)
{
    switch (type)
    {
    case ShaderResourceType::TextureSrv:
    case ShaderResourceType::StructuredBufferSrv:
    case ShaderResourceType::ByteAddressBufferSrv:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    case ShaderResourceType::TextureUav:
    case ShaderResourceType::StructuredBufferUav:
    case ShaderResourceType::ByteAddressBufferUav:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

    case ShaderResourceType::ConstantBuffer:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

    case ShaderResourceType::SamplerState:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

    case ShaderResourceType::AccelerationStructure:
        // TODO: IMPLEMENT
        MIZU_UNREACHABLE("Resource type not implemented");
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    case ShaderResourceType::PushConstant:
        MIZU_UNREACHABLE("Invalid resource type");
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    }
}

} // namespace Mizu::Dx12