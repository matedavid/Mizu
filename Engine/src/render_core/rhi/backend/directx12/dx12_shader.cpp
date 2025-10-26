#include "dx12_shader.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/io/filesystem.h"

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

    {
        // HACK: For the moment, reflection only works with spirv; load spirv version

        const std::filesystem::path& spirv_path = m_description.path.replace_extension(".spv");
        MIZU_ASSERT(std::filesystem::exists(spirv_path), "Path does not exist: {}", spirv_path.string());

        const auto spirv_source = Filesystem::read_file(spirv_path);
        m_reflection = std::make_unique<ShaderReflection>(spirv_source);
    }
}

Dx12Shader::~Dx12Shader()
{
    // Destructor here to destruct m_reflection correctly
}

const ShaderReflection& Dx12Shader::get_reflection() const
{
    return *m_reflection;
}

{
}

} // namespace Mizu::Dx12