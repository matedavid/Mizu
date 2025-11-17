#include "dx12_shader.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/io/filesystem.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/shader/shader_reflection.h"
#include "render_core/shader/shader_transpiler.h"

namespace Mizu::Dx12
{

Dx12Shader::Dx12Shader(Description desc) : m_description(std::move(desc))
{
    m_source_code = Filesystem::read_file(m_description.path);

    m_shader_bytecode = {};
    m_shader_bytecode.pShaderBytecode = m_source_code.data();
    m_shader_bytecode.BytecodeLength = m_source_code.size();

    const std::filesystem::path reflection_path = m_description.path.string() + ".json";
    MIZU_ASSERT(std::filesystem::exists(reflection_path), "Reflection path does not exist");

    const std::string reflection_content = Filesystem::read_file_string(reflection_path);
    m_reflection = std::make_unique<SlangReflection>(reflection_content);
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

const SlangReflection& Dx12Shader::get_reflection() const
{
    return *m_reflection;
}

} // namespace Mizu::Dx12