#pragma once

#include <memory>
#include <vector>

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/shader.h"
#include "render_core/shader/shader_types.h"

namespace Mizu::Dx12
{

class Dx12Shader : public Shader
{
  public:
    Dx12Shader(ShaderDescription desc);
    ~Dx12Shader() override;

    D3D12_SHADER_BYTECODE get_shader_bytecode() const { return m_shader_bytecode; }

    static D3D12_SHADER_VISIBILITY get_dx12_shader_stage_bits(ShaderType stage);
    static D3D12_DESCRIPTOR_RANGE_TYPE get_dx12_descriptor_type(const ShaderResourceT& value);

    const std::string& get_entry_point() const override { return m_description.entry_point; }
    ShaderType get_type() const override { return m_description.type; }

    const SlangReflection& get_reflection() const override;

  private:
    std::vector<char> m_source_code;
    D3D12_SHADER_BYTECODE m_shader_bytecode;

    ShaderDescription m_description{};
    std::unique_ptr<SlangReflection> m_reflection{};
};

} // namespace Mizu::Dx12