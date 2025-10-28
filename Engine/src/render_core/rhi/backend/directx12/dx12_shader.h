#pragma once

#include <memory>
#include <vector>

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/shader.h"

namespace Mizu::Dx12
{

class Dx12Shader : public Shader
{
  public:
    Dx12Shader(Description desc);
    ~Dx12Shader() override;

    D3D12_SHADER_BYTECODE get_shader_bytecode() const { return m_shader_bytecode; }

    const std::string& get_entry_point() const override { return m_description.entry_point; }
    ShaderType get_type() const override { return m_description.type; }

    const SlangReflection& get_reflection() const override;

  private:
    std::vector<char> m_source_code;
    D3D12_SHADER_BYTECODE m_shader_bytecode;

    Shader::Description m_description{};
    std::unique_ptr<SlangReflection> m_reflection{};
};

} // namespace Mizu::Dx12