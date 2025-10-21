#pragma once

#include <memory>
#include <vector>

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{
// Forward declarations
class ShaderReflection;
} // namespace Mizu

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

    const std::vector<ShaderProperty>& get_properties() const override;
    const std::vector<ShaderConstant>& get_constants() const override;
    const std::vector<ShaderInput>& get_inputs() const override;
    const std::vector<ShaderOutput>& get_outputs() const override;

  private:
    Shader::Description m_description{};
    std::unique_ptr<ShaderReflection> m_reflection{nullptr};

    std::vector<char> m_source_code;
    D3D12_SHADER_BYTECODE m_shader_bytecode;
};

} // namespace Mizu::Dx12